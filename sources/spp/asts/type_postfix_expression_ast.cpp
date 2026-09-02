module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_postfix_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
spp::asts::TypePostfixExpressionAst::TypePostfixExpressionAst(
  decltype(Lhs) lhs,
  decltype(TokOp) tok_op) :
  Lhs(std::move(lhs)),
  TokOp(std::move(tok_op)) {
}

spp::asts::TypePostfixExpressionAst::~TypePostfixExpressionAst() = default;

auto spp::asts::TypePostfixExpressionAst::operator<=>(
  TypePostfixExpressionAst const &other) const
  -> Ordering {
  return EqualsTypePostfixExpression(other);
}

auto spp::asts::TypePostfixExpressionAst::operator==(
  TypePostfixExpressionAst const &other) const
  -> bool {
  return EqualsTypePostfixExpression(other) == Ordering::equal;
}

auto spp::asts::TypePostfixExpressionAst::EqualsTypePostfixExpression(
  TypePostfixExpressionAst const &other) const
  -> Ordering {
  // Check the lhs and operator are the same.
  if (*Lhs == *other.Lhs && *TokOp == *other.TokOp) {
    return Ordering::equal;
  }
  return Ordering::less;
}

auto spp::asts::TypePostfixExpressionAst::Equals(
  const ExpressionAst &other) const
  -> Ordering {
  // Double dispatch to the appropriate equals method.
  return other.EqualsTypePostfixExpression(*this);
}

auto spp::asts::TypePostfixExpressionAst::PosStart() const
  -> std::size_t {
  // Use the lhs.
  return Lhs->PosStart();
}

auto spp::asts::TypePostfixExpressionAst::PosEnd() const
  -> std::size_t {
  // Use the operator.
  return TokOp->PosEnd();
}

auto spp::asts::TypePostfixExpressionAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TypePostfixExpressionAst>(
    AstClone(Lhs),
    AstClone(TokOp));
}

auto spp::asts::TypePostfixExpressionAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Lhs);
  SPP_STRING_APPEND(TokOp);
  SPP_STRING_END;
}

auto spp::asts::TypePostfixExpressionAst::Stage4_QualifyTypes(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  (void)sm;
  (void)meta;
  // TODO ?
}

auto spp::asts::TypePostfixExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppAmbiguousMemberAccessError;

  // Move through the left-hand-side type.
  Lhs->Stage7_AnalyseSemantics(sm, meta);
  const auto scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
  const auto lhs_type = Lhs->InferType(sm, meta);
  const auto lhs_type_sym = scope->GetTypeSymbol(lhs_type.get());
  const auto lhs_type_scope = lhs_type_sym->LinkedScope;

  // Check there is only 1 target field on the lhs at the highest level.
  const auto op_nested = TokOp->ToUnchecked<TypePostfixExpressionOperatorNestedTypeAst>();
  auto sup_scopes = lhs_type_sym->LinkedScope->SupScopes();
  sup_scopes.Insert(sup_scopes.begin(), lhs_type_sym->LinkedScope);
  auto scopes_and_syms = sup_scopes
    | genex::views::transform([name=op_nested->Name.get()](auto &&x) {
      return MakePair(x, x->GetTypeSymbol(name, true));
    })
    | genex::to<Vec>()
    | genex::views::filter([](auto &&x) { return x.second != nullptr; })
    | genex::views::transform([&](auto &&x) {
      return MakeTuple(lhs_type_sym->LinkedScope->DepthDiff(x.first), x.first, x.second);
    })
    | genex::to<Vec>();

  auto min_depth = scopes_and_syms.IsEmpty()
    ? 0
    : genex::min_element(
      scopes_and_syms | genex::views::transform([](auto &&x) { return spp::get<0>(x); }) | genex::to<Vec>());

  auto closest = scopes_and_syms
    | genex::views::filter([min_depth](auto &&x) { return spp::get<0>(x) == min_depth; })
    | genex::views::transform([](auto &&x) { return MakePair(spp::get<1>(x), spp::get<2>(x)); })
    | genex::to<Vec>();

  // Can't use raise_if because closest[1] may be out of bounds.
  if (closest.Len() > 1) {
    Raise<SppAmbiguousMemberAccessError>(
      {closest[0].first, closest[1].first, sm->CurrentScope},
      ERR_ARGS(*closest[0].second->Name, *closest[1].second->Name, *op_nested->Name));
  }

  // Ensure the type exists on the "lhs" part.
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->TypeAnalysisTypeScope = lhs_type_scope;
  op_nested->Name->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::TypePostfixExpressionAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // These are always "zero_type", so return init.
  const auto mock_init = MakeUnique<ObjectInitializerAst>(AstClone(this), nullptr);
  return mock_init->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::TypePostfixExpressionAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Infer the type of the left-hand-side.
  Lhs->Stage7_AnalyseSemantics(sm, meta);
  const auto lhs_type = Lhs->InferType(sm, meta);
  const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type.get());
  const auto lhs_type_scope = lhs_type_sym->LinkedScope;

  // Infer the type of the postfix operation.
  const auto op_nested = TokOp->ToUnchecked<TypePostfixExpressionOperatorNestedTypeAst>();
  const auto part = analyse::utils::type_utils::GetTypeSymOrError(
    *lhs_type_scope, *op_nested->Name, *sm)->FqName();
  const auto sym = lhs_type_scope->GetTypeSymbol(part.get());
  return sym->FqName();
}

auto spp::asts::TypePostfixExpressionAst::AnyPart(
  std::function<bool(TypeIdentifierAst const&)> const &pred) const
  -> bool {
  // Walk from the left-hand-side.
  return Lhs->AnyPart(pred);
}

auto spp::asts::TypePostfixExpressionAst::IsNeverType() const noexcept
  -> bool {
  return false;
}

auto spp::asts::TypePostfixExpressionAst::NsParts() const
  -> Vec<IdentifierAst const*> {
  // Concatenate the lhs and rhs namespace parts.
  auto parts = Vec<IdentifierAst const*>();
  NsPartsInto(parts);
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::NsParts()
  -> Vec<IdentifierAst*> {
  // Concatenate the lhs and rhs namespace parts.
  auto parts = Lhs->NsParts();
  parts.AppendRange(TokOp->NsParts());
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::TypeParts() const
  -> Vec<TypeIdentifierAst const*> {
  // Concatenate the lhs and rhs type parts.
  auto parts = Vec<TypeIdentifierAst const*>();
  TypePartsInto(parts);
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::TypeParts()
  -> Vec<TypeIdentifierAst*> {
  // Concatenate the lhs and rhs type parts.
  auto parts = Lhs->TypeParts();
  parts.AppendRange(TokOp->TypeParts());
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::LastTypePart() const
  -> TypeIdentifierAst const* {
  // The operator's part (if any) is appended last; otherwise the final part comes from the lhs.
  if (auto const *op_part = std::as_const(*TokOp).LastTypePart()) { return op_part; }
  return std::as_const(*Lhs).LastTypePart();
}

auto spp::asts::TypePostfixExpressionAst::LastTypePart()
  -> TypeIdentifierAst* {
  if (auto *op_part = TokOp->LastTypePart()) { return op_part; }
  return Lhs->LastTypePart();
}

auto spp::asts::TypePostfixExpressionAst::WithoutConvention() const
  -> Shared<const TypeAst> {
  return shared_from_this();
}

auto spp::asts::TypePostfixExpressionAst::GetConvention() const
  -> ConventionAst* {
  // This type AST will never have a convention directly applied to it.
  return nullptr;
}

auto spp::asts::TypePostfixExpressionAst::WithConvention(
  Unique<ConventionAst> &&conv) const
  -> Shared<TypeAst> {
  if (conv == nullptr) { return const_cast<TypePostfixExpressionAst*>(this)->shared_from_this(); }
  auto borrow_op = MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv));
  auto wrapped = MakeShared<TypeUnaryExpressionAst>(std::move(borrow_op), AstClone(this));
  return wrapped;
}

auto spp::asts::TypePostfixExpressionAst::WithoutGenerics() const
  -> Shared<TypeAst> {
  const auto rhs = TokOp->ToUnchecked<TypePostfixExpressionOperatorNestedTypeAst>();
  auto new_lhs = AstClone(Lhs); // Todo: clone needed?
  auto new_rhs = MakeUnique<TypePostfixExpressionOperatorNestedTypeAst>(
    nullptr, dynamic_shared_cast<TypeIdentifierAst>(rhs->Name->WithoutGenerics()));
  return MakeShared<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}

auto spp::asts::TypePostfixExpressionAst::SubstituteGenerics(
  Vec<GenericArgumentAst*> const &args) const
  -> Shared<TypeAst> {
  const auto rhs = TokOp->ToUnchecked<TypePostfixExpressionOperatorNestedTypeAst>();
  auto new_lhs = Lhs->SubstituteGenerics(args);
  auto new_rhs = MakeUnique<TypePostfixExpressionOperatorNestedTypeAst>(
    nullptr, dynamic_shared_cast<TypeIdentifierAst>(rhs->Name->SubstituteGenerics(args)));
  return MakeShared<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}

auto spp::asts::TypePostfixExpressionAst::ContainsGenerics(
  GenericParameterAst const &generic) const
  -> bool {
  const auto rhs = TokOp->ToUnchecked<TypePostfixExpressionOperatorNestedTypeAst>();
  return rhs->Name->ContainsGenerics(generic);
}

auto spp::asts::TypePostfixExpressionAst::WithGenerics(
  Unique<GenericArgumentGroupAst> &&arg_group) const
  -> Shared<TypeAst> {
  // Clone this type and add the generics to the right most part.
  auto type_clone = AstClone(this);
  type_clone->LastTypePart()->GnArgGroup = std::move(arg_group);
  return type_clone;
}

auto spp::asts::TypePostfixExpressionAst::IsCompilerGeneratedType() const
  -> bool {
  // Won't ever be true.
  return false;
}

auto spp::asts::TypePostfixExpressionAst::ResetCache()
  -> void {
  // Forward into the LHS to reach the inner TypeIdentifierAst.
  Lhs->ResetCache();
}

auto spp::asts::TypePostfixExpressionAst::NsPartsInto(
  Vec<IdentifierAst const*> &out) const
  -> void {
  // Both sides append into the caller's buffer, so a chain of any depth is one allocation.
  std::as_const(*Lhs).NsPartsInto(out);
  std::as_const(*TokOp).NsPartsInto(out);
}

auto spp::asts::TypePostfixExpressionAst::TypePartsInto(
  Vec<TypeIdentifierAst const*> &out) const
  -> void {
  // Both sides append into the caller's buffer, so a chain of any depth is one allocation.
  std::as_const(*Lhs).TypePartsInto(out);
  std::as_const(*TokOp).TypePartsInto(out);
}

SPP_MOD_END
