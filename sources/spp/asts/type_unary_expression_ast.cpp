module;
#include <spp/macros.hpp>

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
import genex;

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
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the RHS type.
    if (const auto op_ns = Op->To<TypeUnaryExpressionOperatorNamespaceAst>()) {
        const auto tm = ScopeManager(sm->GlobalScope, meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope);
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
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the RHS type.
    if (const auto op_ns = Op->To<TypeUnaryExpressionOperatorNamespaceAst>()) {
        const auto tm = ScopeManager(sm->GlobalScope, meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope);
        const auto type_scope = analyse::utils::type_utils::GetNsScopeOrError(*tm.CurrentScope, *op_ns->Ns, tm);
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
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Infer the RHS type.
    const auto type_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
    const auto type_sym = type_scope->GetTypeSymbol(AstClone(this)); // Temp
    return type_sym->FqName()->WithConvention(AstClone(GetConvention()));
}

auto spp::asts::TypeUnaryExpressionAst::Iterator() const
    -> Vec<Shared<const TypeIdentifierAst>> {
    // Iterate from the right-hand-side.
    return Rhs->Iterator();
}

auto spp::asts::TypeUnaryExpressionAst::IsNeverType() const
    -> bool {
    return false;
}

auto spp::asts::TypeUnaryExpressionAst::NsParts() const
    -> Vec<Shared<const IdentifierAst>> {
    auto parts = const_shared_cast(Op)->NsParts();
    parts.AppendRange(const_shared_cast(Rhs)->NsParts());
    return parts;
}

auto spp::asts::TypeUnaryExpressionAst::NsParts()
    -> Vec<Shared<IdentifierAst>> {
    auto parts = Op->NsParts();
    parts.AppendRange(Rhs->NsParts());
    return parts;
}

auto spp::asts::TypeUnaryExpressionAst::TypeParts() const
    -> Vec<Shared<const TypeIdentifierAst>> {
    auto parts = const_shared_cast(Op)->TypeParts();
    parts.AppendRange(const_shared_cast(Rhs)->TypeParts());
    return parts;
}

auto spp::asts::TypeUnaryExpressionAst::TypeParts()
    -> Vec<Shared<TypeIdentifierAst>> {
    auto parts = Op->TypeParts();
    parts.AppendRange(Rhs->TypeParts());
    return parts;
}

auto spp::asts::TypeUnaryExpressionAst::WithoutConvention() const
    -> Shared<const TypeAst> {
    if (Op->To<TypeUnaryExpressionOperatorBorrowAst>() != nullptr) {
        return Rhs;
    }
    return shared_from_this();
}

auto spp::asts::TypeUnaryExpressionAst::GetConvention() const
    -> ConventionAst* {
    if (auto const *op_borrow = Op->To<TypeUnaryExpressionOperatorBorrowAst>()) {
        return op_borrow->Conv.get();
    }
    return nullptr;
}

auto spp::asts::TypeUnaryExpressionAst::WithConvention(
    Unique<ConventionAst> &&conv) const
    -> Shared<TypeAst> {
    if (conv == nullptr) {
        return MakeShared<TypeUnaryExpressionAst>(Op, Rhs);
    }
    if (Op->To<TypeUnaryExpressionOperatorBorrowAst>()) {
        return MakeShared<TypeUnaryExpressionAst>(
            MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)),
            Rhs);
    }
    return MakeShared<TypeUnaryExpressionAst>(
        MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)),
        MakeShared<TypeUnaryExpressionAst>(Op, Rhs));
}

auto spp::asts::TypeUnaryExpressionAst::WithoutGenerics() const
    -> Shared<TypeAst> {
    // Todo: using the cache breaks.
    return MakeShared<TypeUnaryExpressionAst>(Op, Rhs->WithoutGenerics());
}

auto spp::asts::TypeUnaryExpressionAst::SubstituteGenerics(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<TypeAst> {
    return MakeShared<TypeUnaryExpressionAst>(AstClone(Op), Rhs->SubstituteGenerics(args));
}

auto spp::asts::TypeUnaryExpressionAst::ContainsGenerics(
    GenericParameterAst const &generic) const
    -> bool {
    return Rhs->ContainsGenerics(generic);
}

auto spp::asts::TypeUnaryExpressionAst::WithGenerics(
    Unique<GenericArgumentGroupAst> &&arg_group) const
    -> Shared<TypeAst> {
    // Clone this type and add the generics to the right most part.
    auto type_clone = AstClone(this);
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
