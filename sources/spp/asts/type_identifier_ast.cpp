module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_identifier_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
import spp.asts.class_prototype_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import ankerl.unordered_dense;

SPP_MOD_BEGIN
auto spp::asts::TypeIdentifierAst::FromIdentifier(
    IdentifierAst const &identifier)
    -> Unique<TypeIdentifierAst> {
    return MakeUnique<TypeIdentifierAst>(identifier.PosStart(), Str(identifier.Val), nullptr);
}

auto spp::asts::TypeIdentifierAst::FromString(
    Str const &identifier)
    -> Unique<TypeIdentifierAst> {
    return MakeUnique<TypeIdentifierAst>(0uz, Str(identifier), nullptr);
}

spp::asts::TypeIdentifierAst::TypeIdentifierAst(
    const std::size_t pos,
    decltype(Name) &&name,
    decltype(GnArgGroup) generic_arg_group) :
    Name(std::move(name)),
    GnArgGroup(std::move(generic_arg_group)),
    _Pos(pos) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
    if (Name == "Self") { _IsSelfType = true; }
}

spp::asts::TypeIdentifierAst::~TypeIdentifierAst() = default;

auto spp::asts::TypeIdentifierAst::operator<=>(
    const TypeIdentifierAst &that) const
    -> Ordering {
    return EqualsTypeIdentifier(that);
}

auto spp::asts::TypeIdentifierAst::operator==(
    const TypeIdentifierAst &that) const
    -> bool {
    return EqualsTypeIdentifier(that) == Ordering::equal;
}

auto spp::asts::TypeIdentifierAst::EqualsTypeIdentifier(
    TypeIdentifierAst const &other) const
    -> Ordering {
    // Equality is based on the name and generics.
    return Name == other.Name and *GnArgGroup == *other.GnArgGroup
        ? Ordering::equal
        : Ordering::less;
}

auto spp::asts::TypeIdentifierAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsTypeIdentifier(*this);
}

auto spp::asts::TypeIdentifierAst::PosStart() const
    -> std::size_t {
    // Use the static pos field.
    return _Pos;
}

auto spp::asts::TypeIdentifierAst::PosEnd() const
    -> std::size_t {
    // Use the final generic argument or name.
    return _Pos + Name.length();
}

auto spp::asts::TypeIdentifierAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto t = MakeUnique<TypeIdentifierAst>(_Pos, Str(Name), AstClone(GnArgGroup));
    t->_IsNeverType = _IsNeverType;
    return t;
}

auto spp::asts::TypeIdentifierAst::ToString() const
    -> Str {
    SPP_STRING_START;
    raw_string.append(Name);
    SPP_STRING_APPEND(GnArgGroup);
    SPP_STRING_END;
}

auto spp::asts::TypeIdentifierAst::Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Qualify the generic argument types.
    for (auto const &g : GnArgGroup->GetTypeArgs()) {
        g->Stage4_QualifyTypes(sm, meta);
    }
}

auto spp::asts::TypeIdentifierAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::func_utils::EnforceGenericConstraintsAllArgs;
    using analyse::utils::func_utils::InferGnArgs;
    using analyse::utils::func_utils::NameGnArgs;
    using analyse::utils::type_utils::CreateGenericClsScope;
    using analyse::utils::type_utils::GetTypeSymOrError;
    if (_HasAnalysed) { return; }
    if (Name == "Self" and meta->CurrentStage < 9) { return; }

    // Determine the scope and get the type symbol.
    const auto scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
    const auto type_sym = GetTypeSymOrError(
        *scope, *WithoutGenerics()->To<TypeIdentifierAst>(), *sm, meta);
    if (Name == "Self") { return; }
    const auto &gn_param_group = type_sym->AliasStmt != nullptr ? type_sym->AliasStmt->GnParamGroup : type_sym->Type->GnParamGroup;

    auto is_tuple = false;
    if (not type_sym->IsGeneric) {
        is_tuple = ( {
            // Keep the owning Unique alive for the comparison; WithoutGenerics() returns a temporary and To<>() is non-owning.
            const auto without_generics = type_sym->FqName()->WithoutGenerics();
            const auto as_unary = without_generics->To<TypeUnaryExpressionAst>();
            as_unary != nullptr and *as_unary == *generate::common_types_precompiled::TUP->To<TypeUnaryExpressionAst>();
        });

        // Name all the generic arguments.
        NameGnArgs(
            *GnArgGroup,
            *(type_sym->AliasStmt != nullptr ? type_sym->AliasStmt->GnParamGroup : type_sym->Type->GnParamGroup),
            *this, *sm, *meta, is_tuple);

        // Analyse the generic arguments.
        if (meta->SkipTypeAnalysisGenericChecks) { return; }
        meta->TypeAnalysisTypeScope = nullptr;
        GnArgGroup->Stage7_AnalyseSemantics(sm, meta);

        // Infer the generic arguments from information given from object initialisation.
        InferGnArgs(
            *gn_param_group, *GnArgGroup, std::move(meta->InferSource), std::move(meta->InferTarget),
            type_sym->FqName(), *type_sym->LinkedScope, nullptr, is_tuple, *sm, *meta);
        GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
    }

    // For variant types, collapse any duplicate generic arguments.
    // if (TypeEq(*without_generics(), *generate::common_types_precompiled::VAR, *scope, *sm->CurrentScope, false)) {
    //     auto inner_types = analyse::utils::type_utils::deduplicate_variant_inner_types(*this, *sm->CurrentScope);
    //     auto inner_types_as_tup = generate::common_types::tuple_type(PosStart(), std::move(inner_types));
    //     meta->Save();
    //     meta->TypeAnalysisTypeScope = type_scope;
    //     inner_types_as_tup->Stage7_AnalyseSemantics(sm, meta);
    //     meta->Restore();
    //     ast_cast<GenericArgumentTypeAst>(generic_arg_group->Args[0].Get())->val = std::move(inner_types_as_tup);
    // }

    // If the generically filled type doesn't exist (Vec[Str]), but the base does (Vec[T]), create it.
    if (not scope->HasTypeSymbol(this)) {
        const auto external_generics = sm->CurrentScope->GetExtendedGenericSymbols(GnArgGroup->GetAllArgs(), meta->IgnoreCmpGeneric);
        CreateGenericClsScope(*this, *type_sym, external_generics, is_tuple, sm, meta);
    }

    // Enforce generic constraints from the pre-analysis stage (CurrentStage >= 8) onwards, not just the main
    // analysis stage. Sup scopes are fully loaded by the end of stage 5, so constraints can be reliably checked here,
    // and some need to be done before stage 7 for order agnostic behaviour.
    if (not GnArgGroup->Args.IsEmpty() and meta->CurrentStage >= 8) {
        EnforceGenericConstraintsAllArgs(*gn_param_group, *GnArgGroup, *sm->CurrentScope, *sm, *meta);
    }

    _HasAnalysed = true;
}

// auto spp::asts::TypeIdentifierAst::Iterator() const
//     -> Vec<TypeIdentifierAst*> {
//     // First yield is the original type being iterated over.
//     auto parts = Vec<TypeIdentifierAst*>();
//     parts.EmplaceBack(this);
//
//     for (auto &&g : GnArgGroup->Args) {
//         // Positional generic comp argument with identifier value.
//         if (auto &&comp_positional_arg = g->To<GenericArgumentCompPositionalAst>()) {
//             if (auto &&ident_val = comp_positional_arg->Val->To<IdentifierAst>()) {
//                 parts.EmplaceBack(FromIdentifier(*ident_val));
//             }
//         }
//
//         // Keyword generic comp argument with identifier value.
//         else if (auto &&comp_keyword_arg = g->To<GenericArgumentCompKeywordAst>()) {
//             if (auto &&ident_val = comp_keyword_arg->Val->To<IdentifierAst>()) {
//                 parts.EmplaceBack(FromIdentifier(*ident_val));
//             }
//         }
//
//         // Positional generic type arguments => recursive iteration.
//         else if (auto &&type_positional_arg = g->To<GenericArgumentTypePositionalAst>()) {
//             for (auto &&ti : type_positional_arg->Val->Iterator()) {
//                 parts.EmplaceBack(ti);
//             }
//         }
//
//         // Keyword generic type arguments => recursive iteration.
//         else if (auto &&type_keyword_arg = g->To<GenericArgumentTypeKeywordAst>()) {
//             for (auto &&ti : type_keyword_arg->Val->Iterator()) {
//                 parts.EmplaceBack(ti);
//             }
//         }
//     }
//
//     return parts;
// }

auto spp::asts::TypeIdentifierAst::IsNeverType() const noexcept
    -> bool {
    return _IsNeverType;
}

auto spp::asts::TypeIdentifierAst::IsSelfType() const noexcept
    -> bool {
    return _IsSelfType;
}

auto spp::asts::TypeIdentifierAst::NsParts() const
    -> Vec<IdentifierAst*> {
    return {};
}

auto spp::asts::TypeIdentifierAst::TypeParts() const
    -> Vec<TypeIdentifierAst*> {
    return Vec{const_cast<TypeIdentifierAst*>(this)};
}

auto spp::asts::TypeIdentifierAst::WithoutConvention() const
    -> TypeAst* {
    return const_cast<TypeIdentifierAst*>(this);
}

auto spp::asts::TypeIdentifierAst::GetConvention() const
    -> ConventionAst* {
    return nullptr;
}

auto spp::asts::TypeIdentifierAst::WithConvention(
    Unique<ConventionAst> &&conv) const
    -> Unique<TypeAst> {
    if (conv == nullptr) { return AstClone(this); }

    auto borrow_op = MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv));
    auto wrapped = MakeUnique<TypeUnaryExpressionAst>(std::move(borrow_op), AstClone(this));
    return wrapped;
}

auto spp::asts::TypeIdentifierAst::WithoutGenerics() const
    -> Unique<TypeAst> {
    return MakeUnique<TypeIdentifierAst>(_Pos, Str(Name), nullptr);
}

auto spp::asts::TypeIdentifierAst::SubstituteGenerics(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<TypeAst> {
    if (args.IsEmpty() or GnArgGroup == nullptr) { return AstClone(this); }

    // Get the generic type and comp arguments, split and transformed.
    auto gen_type_args = Vec<Pair<TypeIdentifierAst*, ExpressionAst*>>{};
    auto gen_comp_args = Vec<Pair<TypeIdentifierAst*, ExpressionAst*>>{};
    for (auto const &arg : args) {
        if (auto const *type_kw_arg = arg->To<GenericArgumentTypeKeywordAst>()) {
            gen_type_args.EmplaceBack(type_kw_arg->Name->To<TypeIdentifierAst>(), type_kw_arg->Val.Get());
        }
        else if (auto const *comp_kw_arg = arg->To<GenericArgumentCompKeywordAst>()) {
            gen_comp_args.EmplaceBack(comp_kw_arg->Name->To<TypeIdentifierAst>(), comp_kw_arg->Val.Get());
        }
    }

    // Check if this type directly matches any generic type argument name.
    for (auto const &[gen_arg_name, gen_arg_val] : gen_type_args) {
        if (*this == *gen_arg_name->To<TypeIdentifierAst>()) {
            return AstClone(gen_arg_val->To<TypeAst>());
        }
    }

    // Substitute generics in the comp arguments' types.
    auto name_clone = AstClone(this);
    const auto comp_args_in_clone = name_clone->GnArgGroup->GetCompArgs();
    for (auto const &[gen_arg_name, gen_arg_val] : gen_comp_args) {
        const auto expected = IdentifierAst::FromType(*gen_arg_name);
        for (auto const &g : comp_args_in_clone) {
            if (auto const *ident_val = g->Val->To<IdentifierAst>(); ident_val != nullptr and *ident_val == *expected) {
                g->Val = AstClone(gen_arg_val);
            }
        }
    }

    // Substitute generics in the type arguments' types.
    for (auto const &g : name_clone->GnArgGroup->GetTypeArgs()) {
        g->Val = g->Val->SubstituteGenerics(args);
    }

    // Return the cloned type with generics substituted.
    return name_clone;
}

auto spp::asts::TypeIdentifierAst::ContainsGenerics(
    GenericParameterAst const &generic) const
    -> bool {
    // Check if this type identifier is the generic parameter itself (e.g. the "T" in "sup [T] T").
    auto cast_name = generic.Name->To<TypeIdentifierAst>();
    if (*this == *cast_name) { return true; }

    // Otherwise recurse into the generic argument group, so nested usages like the "T" in "BaseClass[T]" are found.
    for (auto const &g : GnArgGroup->Args) {
        if (auto const *comp_positional_arg = g->To<GenericArgumentCompPositionalAst>(); comp_positional_arg != nullptr) {
            if (auto const *ident_val = comp_positional_arg->Val->To<IdentifierAst>(); ident_val != nullptr and *FromIdentifier(*ident_val) == *cast_name) { return true; }
        }
        else if (auto const *comp_keyword_arg = g->To<GenericArgumentCompKeywordAst>(); comp_keyword_arg != nullptr) {
            if (auto const *ident_val = comp_keyword_arg->Val->To<IdentifierAst>(); ident_val != nullptr and *FromIdentifier(*ident_val) == *cast_name) { return true; }
        }
        else if (auto const *type_positional_arg = g->To<GenericArgumentTypePositionalAst>(); type_positional_arg != nullptr) {
            if (type_positional_arg->Val->ContainsGenerics(generic)) { return true; }
        }
        else if (auto const *type_keyword_arg = g->To<GenericArgumentTypeKeywordAst>(); type_keyword_arg != nullptr) {
            if (type_keyword_arg->Val->ContainsGenerics(generic)) { return true; }
        }
    }
    return false;
}

auto spp::asts::TypeIdentifierAst::WithGenerics(
    Unique<GenericArgumentGroupAst> &&arg_group) const
    -> Unique<TypeAst> {
    // Attach the new generic argument group to a clone of this type identifier.
    arg_group = arg_group != nullptr ? std::move(arg_group) : GenericArgumentGroupAst::NewEmpty();
    return MakeUnique<TypeIdentifierAst>(_Pos, Str(Name), std::move(arg_group));
}

auto spp::asts::TypeIdentifierAst::IsCompilerGeneratedType() const
    -> bool {
    // Types starting with "$" are compiler generated (not parsable).
    return Name[0] == '$';
}

auto spp::asts::TypeIdentifierAst::ResetCache()
    -> void {
    // Reset the cache to allow overriding the analysis skipper instruction.
    _HasAnalysed = false;
}

auto spp::asts::TypeIdentifierAst::IsTypeIdentifier() const noexcept
    -> bool {
    return true;
}

auto spp::asts::TypeIdentifierAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Fully qualify this type name from the scope.
    // Have to AstClone because PostfixExpressionAst lhs (will change with removal of all shared pointers)
    const auto type_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
    const auto type_sym = type_scope->GetTypeSymbol(this);
    auto inferred = AstClone(type_sym->FqName());
    CACHE_TYPE_INFERENCE_AND_RETURN(inferred);
}

auto spp::asts::TypeIdentifierAst::AnkerlHash() const
    -> std::size_t {
    // Hash based on the name only.
    return ankerl::unordered_dense::hash<Str>()(Name);
}

auto spp::asts::TypeIdentifierAst::ToView() const
    -> StrView {
    if (_CachedStringification.empty()) {
        _CachedStringification = Name;
        if (GnArgGroup != nullptr) {
            _CachedStringification.append(GnArgGroup->ToString());
        }
    }
    return _CachedStringification;
}

SPP_MOD_END
