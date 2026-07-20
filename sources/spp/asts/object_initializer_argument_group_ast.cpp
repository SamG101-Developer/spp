module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_argument_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.visibility_utils;
import spp.asts.class_attribute_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
auto spp::asts::ObjectInitializerArgumentGroupAst::NewEmpty()
    -> Unique<ObjectInitializerArgumentGroupAst> {
    // Factory for an empty object initializer group.
    return MakeUnique<ObjectInitializerArgumentGroupAst>(
        nullptr, decltype(Args)(), nullptr);
}

spp::asts::ObjectInitializerArgumentGroupAst::ObjectInitializerArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Args(std::move(args)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}

spp::asts::ObjectInitializerArgumentGroupAst::~ObjectInitializerArgumentGroupAst() = default;

auto spp::asts::ObjectInitializerArgumentGroupAst::PosStart() const
    -> std::size_t {
    // Use the "{" token.
    return TokL->PosStart();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::PosEnd() const
    -> std::size_t {
    // Use the "}" token.
    return TokR->PosEnd();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ObjectInitializerArgumentGroupAst>(
        AstClone(TokL), AstCloneVec(Args), AstClone(TokR));
}

auto spp::asts::ObjectInitializerArgumentGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Args, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppArgumentNameInvalidError;
    using analyse::errors::SppIdentifierDuplicateError;
    using analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError;
    using analyse::utils::type_utils::GetAllAttrs;

    const auto all_attrs = GetAllAttrs(*meta->ObjectInitType, sm);
    const auto all_attr_names = all_attrs
        | genex::views::tuple_nth<0>
        | genex::to<Vec>();

    // Check there is at most 1 autofill argument.
    const auto af_args = GetShorthandArgs()
        | genex::views::filter([](auto const &x) { return x->TokEllipsis != nullptr; })
        | genex::to<Vec>();

    RaiseIf<SppObjectInitializerMultipleAutofillArgumentsError>(
        af_args.Len() > 1, {sm->CurrentScope},
        ERR_ARGS(*af_args[0], *af_args[1]));

    // Ensure there are no duplicate member names. This needs to be done before semantic analysis as other ASTs might
    // try reading a duplicate attribute before an error is raised.
    const auto duplicates = GetKeywordArgs()
        | genex::views::transform([](auto const &x) { return x->Name; })
        | genex::to<Vec>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<Vec>();

    RaiseIf<SppIdentifierDuplicateError>(
        not duplicates.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*duplicates[0], *duplicates[1], "keyword object initializer argument"));

    // Check there are no invalidly named arguments.
    auto arg_names = GetNonAutoFillArgs()
        | genex::views::transform([](auto const &x) { return x->Name.get(); })
        | genex::views::filter([](auto const *x) { return x != nullptr; }) // Filter bad unnamed args checked after.
        | genex::to<Vec>();

    const auto invalid_args = arg_names
        | genex::views::not_in(all_attr_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

    RaiseIf<SppArgumentNameInvalidError>(
        not invalid_args.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*meta->ObjectInitType, "attribute", *invalid_args[0], "object initializer argument"));

    // Analyse the arguments in the group.
    for (auto const &arg : Args) {
        // Return type overload helper.
        meta->Save();
        if (const auto kw_arg = arg->To<ObjectInitializerArgumentKeywordAst>(); kw_arg != nullptr) {
            SPP_RETURN_TYPE_OVERLOAD_HELPER(arg->Val.get()) {
                // Multiple attributes with same name (via base classes) -> can't infer the one to use.
                auto attrs = all_attrs
                    | genex::views::filter([kw_arg](auto const &x) { return *std::get<0>(x) == *kw_arg->Name; })
                    | genex::to<Vec>();
                if (attrs.Len() > 1) { continue; }

                // Use the type off the single matching attribute.
                const auto attr_type_sym = std::get<1>(attrs[0]);
                const auto attr_type = attr_type_sym->IsGeneric ? nullptr : attr_type_sym->FqName();
                meta->ReturnTypeOverloadResolverType = std::move(attr_type);
            }
        }

        arg->Stage7_AnalyseSemantics(sm, meta);
        meta->Restore();
    }
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::type_utils::TypeEq;
    using analyse::utils::type_utils::GetAllAttrs;
    using analyse::utils::type_utils::GetAllAttrAsts;
    using analyse::utils::type_utils::IsTypeVariant;
    using analyse::utils::visibility_utils::CheckTypeMemberVisibility;
    using analyse::errors::SemanticError;
    using analyse::errors::SppAmbiguousMemberAccessError;
    using analyse::errors::SppGeneratedCodeError;
    using analyse::errors::SppObjectInitializerVariantError;
    using analyse::errors::SppTypeMismatchError;

    // Remove any compiler generated args for re-analysis (generically).
    Args |= genex::actions::remove_if([](auto const &x) { return x->IsCompilerGenerated; });

    // Get the attributes on the type and supertypes.
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(meta->ObjectInitType);
    const auto all_attrs = GetAllAttrs(*meta->ObjectInitType, sm);

    // Type check the non-autofill arguments against the class attributes.
    for (auto const &arg : GetNonAutoFillArgs()) {
        auto matching_attrs = all_attrs
            | genex::views::filter([&arg](auto const &x) { return *std::get<0>(x) == *arg->Name; })
            | genex::to<Vec>();

        RaiseIf<SppAmbiguousMemberAccessError>(
            matching_attrs.Len() > 1, {sm->CurrentScope},
            ERR_ARGS(*std::get<0>(matching_attrs[0]), *std::get<0>(matching_attrs[1]), *this));

        auto [attr, attr_type_sym, _] = matching_attrs[0];

        // Enforce visibility on the field being initialized.
        {
            const auto scope = cls_sym->LinkedScope->NonGenericScope;
            const auto sym = scope->GetVarSymbol(arg->Name, true);
            CheckTypeMemberVisibility(*sym, *arg->Name, *scope, *sm, *meta);
        }

        const auto attr_type = attr_type_sym->FqName();
        meta->Save();
        meta->AssignmentTargetType = attr_type;
        meta->AssignmentTarget = IdentifierAst::FromType(*meta->AssignmentTargetType);
        auto arg_type = arg->InferType(sm, meta);
        meta->Restore();

        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*attr_type, *arg_type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*attr, *attr_type, *arg, *arg_type));
    }

    // Type check the default argument (if it exists).
    const auto af_arg = GetAutoFillArg();
    if (af_arg != nullptr) {
        const auto af_arg_type = af_arg->Val->InferType(sm, meta);
        RaiseIf<SppTypeMismatchError>( // Todo: pass a "meta->SourceObjectInitType" or just pass a "meta->ObjectInit"
            not TypeEq(*af_arg_type, *meta->ObjectInitType, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*meta->ObjectInitType, *meta->ObjectInitType, *af_arg, *af_arg_type));
    }

    // Generate an argument for every attribute the user didn't pass.
    const auto all_attr_asts = GetAllAttrAsts(*meta->ObjectInitType, sm);
    const auto given_names = GetNonAutoFillArgs()
        | genex::views::transform([](auto const &x) { return x->Name; })
        | genex::to<Vec>();

    for (auto i = 0uz; i < all_attrs.Len(); ++i) {
        auto const &[attr_name, attr_type_sym, attr_parent_scope] = all_attrs[i];

        // Todo: this will fail when multiple bases classes have same attr name
        // Todo: maybe we just block this? there's currently no way to refer to individual base class's fields.
        if (genex::any_of(given_names, [&attr_name](auto const &x) { return *x == *attr_name; })) { continue; }

        const auto attr_ast = all_attr_asts[i];
        auto val = Unique<ExpressionAst>();
        auto val_scope = static_cast<analyse::scopes::Scope*>(nullptr);

        if (af_arg != nullptr) {
            // Get the attribute from the autofill argument => "..other" becomes "attr=other.attr".
            auto member = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, attr_name);
            val = MakeUnique<PostfixExpressionAst>(AstClone(af_arg->Val), std::move(member));
            val_scope = sm->CurrentScope;
        }
        else if (attr_ast->DefaultVal != nullptr) {
            // If the attribute provides a default value, use it there. This then loads to codegen later.
            val = AstClone(attr_ast->DefaultVal);
            val_scope = cls_sym->LinkedScope;
        }
        else {
            // Otherwise default initialize the attribute with an empty object initializer over its type.
            auto obj_init = MakeUnique<ObjectInitializerAst>(attr_type_sym->FqName(), nullptr);
            obj_init->Source.OriginalType = attr_ast->Source.OriginalType;
            val = std::move(obj_init);
            val_scope = attr_parent_scope; // Whilst this seems like it should be current scope, the type is FQ and we need error reporting
        }

        // Todo: Might need "tm" here? Otherwise switch program-wide "tm" to this pattern.
        // Todo: Actually, make a method on the scope manager: "TempIn(scope, closure)".
        const auto outer_scope = sm->CurrentScope;
        sm->CurrentScope = val_scope;
        WRAP_ERROR(val->Stage7_AnalyseSemantics(sm, meta), outer_scope);
        sm->CurrentScope = outer_scope;

        // Add the compiler generated argument.
        auto arg = MakeUnique<ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(val));
        arg->IsCompilerGenerated = true;
        Args.EmplaceBack(std::move(arg));
    }

    // The autofill argument has been expanded into one argument per attribute, so it isn't needed any more.
    if (af_arg != nullptr) {
        Args |= genex::actions::remove_if([af_arg](auto const &x) { return x.get() == af_arg; });
    }
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the arguments.
    for (auto const &arg : Args) { arg->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetAllArgs()
    -> Vec<ObjectInitializerArgumentAst*> {
    // Get all arguments in the group.
    return Args
        | genex::views::ptr
        | genex::to<Vec>();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetAutoFillArg()
    -> ObjectInitializerArgumentShorthandAst* {
    // Get the first shorthand argument tagged with the ".." token.
    const auto filtered = Args
        | genex::views::ptr
        | genex::views::cast_dynamic<ObjectInitializerArgumentShorthandAst*>()
        | genex::views::filter([](auto const &x) { return x != nullptr and x->TokEllipsis != nullptr; })
        | genex::to<Vec>();
    return filtered.IsEmpty() ? nullptr : filtered[0];
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetNonAutoFillArgs()
    -> Vec<ObjectInitializerArgumentAst*> {
    // Get the first shorthand argument tagged with the ".." token.
    return Args
        | genex::views::ptr
        | genex::views::remove(GetAutoFillArg())
        | genex::to<Vec>();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetShorthandArgs()
    -> Vec<ObjectInitializerArgumentShorthandAst*> {
    // Get all shorthand arguments tagged with the ".." token.
    return Args
        | genex::views::ptr
        | genex::views::cast_dynamic<ObjectInitializerArgumentShorthandAst*>()
        | genex::to<Vec>();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetKeywordArgs()
    -> Vec<ObjectInitializerArgumentKeywordAst*> {
    // Get all keyword arguments.
    return Args
        | genex::views::ptr
        | genex::views::cast_dynamic<ObjectInitializerArgumentKeywordAst*>()
        | genex::to<Vec>();
}

SPP_MOD_END
