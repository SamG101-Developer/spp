module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_unary_expression_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_unary_expression_operator_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;

SPP_MOD_BEGIN
spp::asts::TypeUnaryExpressionAst::TypeUnaryExpressionAst(
    decltype(Op) op,
    decltype(Rhs) rhs) :
    Op(std::move(op)),
    Rhs(std::move(rhs)) {
}

spp::asts::TypeUnaryExpressionAst::~TypeUnaryExpressionAst() = default;

auto spp::asts::TypeUnaryExpressionAst::operator<=>(
    TypeUnaryExpressionAst const &other) const
    -> Ordering {
    return EqualsTypeUnaryExpression(other);
}

auto spp::asts::TypeUnaryExpressionAst::operator==(
    TypeUnaryExpressionAst const &other) const
    -> bool {
    return EqualsTypeUnaryExpression(other) == Ordering::equal;
}

auto spp::asts::TypeUnaryExpressionAst::EqualsTypeUnaryExpression(
    TypeUnaryExpressionAst const &other) const
    -> Ordering {
    // Equality is based on the operator and rhs.
    return *Op == *other.Op && *Rhs == *other.Rhs ? Ordering::equal : Ordering::less;
}

auto spp::asts::TypeUnaryExpressionAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsTypeUnaryExpression(*this);
}

auto spp::asts::TypeUnaryExpressionAst::PosStart() const
    -> std::size_t {
    // Use the operator.
    return Op->PosStart();
}

auto spp::asts::TypeUnaryExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the rhs.
    return Rhs->PosEnd();
}

auto spp::asts::TypeUnaryExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeUnaryExpressionAst>(AstClone(Op), AstClone(Rhs));
}

auto spp::asts::TypeUnaryExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Op);
    SPP_STRING_APPEND(Rhs);
    SPP_STRING_END;
}

auto spp::asts::TypeUnaryExpressionAst::Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Qualify the RHS type.
    if (const auto op_ns = Op->To<TypeUnaryExpressionOperatorNamespaceAst>()) {
        const auto tm = analyse::scopes::ScopeManager(sm->GlobalScope, meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope);
        const auto type_scope = analyse::utils::type_utils::GetNsScopeOrError(*tm.CurrentScope, *op_ns->Ns, tm);
        meta->Save();
        meta->TypeAnalysisTypeScope = type_scope;
        Rhs->Stage4_QualifyTypes(sm, meta);
        meta->Restore();
    }
    else {
        Rhs->Stage4_QualifyTypes(sm, meta);
    }
}

auto spp::asts::TypeUnaryExpressionAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Analyse the RHS type.
    if (const auto op_ns = Op->To<TypeUnaryExpressionOperatorNamespaceAst>()) {
        const auto tm = analyse::scopes::ScopeManager(sm->GlobalScope, meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope);
        const auto type_scope = analyse::utils::type_utils::GetNsScopeOrError(*tm.CurrentScope, *op_ns->Ns, *sm);
        meta->Save();
        meta->TypeAnalysisTypeScope = type_scope;
        Rhs->Stage7_AnalyseSemantics(sm, meta);
        meta->Restore();
    }
    else {
        Rhs->Stage7_AnalyseSemantics(sm, meta);
    }
}

auto spp::asts::TypeUnaryExpressionAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Infer the RHS type.
    const auto type_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
    const auto type_sym = type_scope->GetTypeSymbol(this);
    auto inferred = type_sym->FqName()->WithConvention(AstClone(GetConvention()));
    CACHE_TYPE_INFERENCE_AND_RETURN(inferred);
}

auto spp::asts::TypeUnaryExpressionAst::IsNeverType() const noexcept
    -> bool {
    return false;
}

auto spp::asts::TypeUnaryExpressionAst::IsSelfType() const noexcept
    -> bool {
    // Allow for &Self to be considered "Self".
    return Op->To<TypeUnaryExpressionOperatorBorrowAst>() and Rhs->IsSelfType();
}

auto spp::asts::TypeUnaryExpressionAst::NsParts() const
    -> Vec<IdentifierAst*> {
    auto parts = Op->NsParts();
    parts.AppendRange(Rhs->NsParts());
    return parts;
}

auto spp::asts::TypeUnaryExpressionAst::TypeParts() const
    -> Vec<TypeIdentifierAst*> {
    auto parts = Op->TypeParts();
    parts.AppendRange(Rhs->TypeParts());
    return parts;
}

auto spp::asts::TypeUnaryExpressionAst::WithoutConvention() const
    -> TypeAst* {
    if (Op->To<TypeUnaryExpressionOperatorBorrowAst>() != nullptr) {
        return Rhs.Get();
    }
    return const_cast<TypeUnaryExpressionAst*>(this);
}

auto spp::asts::TypeUnaryExpressionAst::GetConvention() const
    -> ConventionAst* {
    if (auto const *op_borrow = Op->To<TypeUnaryExpressionOperatorBorrowAst>()) {
        return op_borrow->Conv.Get();
    }
    return nullptr;
}

auto spp::asts::TypeUnaryExpressionAst::WithConvention(
    Unique<ConventionAst> &&conv) const
    -> Unique<TypeAst> {
    if (conv == nullptr and Op->To<TypeUnaryExpressionOperatorBorrowAst>() != nullptr) {
        // Remove the convention
        return AstClone(Rhs);
    }
    if (conv == nullptr) {
        return MakeUnique<TypeUnaryExpressionAst>(AstClone(Op), AstClone(Rhs));
    }
    if (Op->To<TypeUnaryExpressionOperatorBorrowAst>()) {
        return MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)), AstClone(Rhs));
    }
    return MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)), MakeUnique<TypeUnaryExpressionAst>(AstClone(Op), AstClone(Rhs)));
}

auto spp::asts::TypeUnaryExpressionAst::WithoutGenerics() const
    -> Unique<TypeAst> {
    return MakeUnique<TypeUnaryExpressionAst>(AstClone(Op), Rhs->WithoutGenerics());
}

auto spp::asts::TypeUnaryExpressionAst::SubstituteGenerics(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<TypeAst> {
    return MakeUnique<TypeUnaryExpressionAst>(AstClone(Op), Rhs->SubstituteGenerics(args));
}

auto spp::asts::TypeUnaryExpressionAst::ContainsGenerics(
    GenericParameterAst const &generic) const
    -> bool {
    return Rhs->ContainsGenerics(generic);
}

auto spp::asts::TypeUnaryExpressionAst::WithGenerics(
    Unique<GenericArgumentGroupAst> &&arg_group) const
    -> Unique<TypeAst> {
    // Clone this type and add the generics to the right most part.
    auto type_clone = AstClone(this);
    arg_group = arg_group != nullptr ? std::move(arg_group) : GenericArgumentGroupAst::NewEmpty();
    type_clone->TypeParts().Back()->GnArgGroup = std::move(arg_group);
    return type_clone;
}

auto spp::asts::TypeUnaryExpressionAst::IsCompilerGeneratedType() const
    -> bool {
    // Move into the rhs, ie for "std::annotations::$Public".
    return Rhs->IsCompilerGeneratedType();
}

auto spp::asts::TypeUnaryExpressionAst::ResetCache()
    -> void {
    // Forward into the RHS to reach the inner TypeIdentifierAst.
    Rhs->ResetCache();
}

SPP_MOD_END
