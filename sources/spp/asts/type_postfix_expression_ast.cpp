module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_postfix_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_resolution;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
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

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::TypePostfixExpressionAst::TypePostfixExpressionAst(
  decltype(Lhs) lhs,
  decltype(TokOp) tok_op) :
  Lhs(std::move(lhs)),
  TokOp(std::move(tok_op)) {
}

spp::asts::TypePostfixExpressionAst::~TypePostfixExpressionAst() = default;

auto spp::asts::TypePostfixExpressionAst::operator<=>(
  TypePostfixExpressionAst const &other) const -> Ordering {
  return EqualsTypePostfixExpression(other);
}

auto spp::asts::TypePostfixExpressionAst::operator==(
  TypePostfixExpressionAst const &other) const -> bool {
  return EqualsTypePostfixExpression(other) == Ordering::equal;
}

auto spp::asts::TypePostfixExpressionAst::EqualsTypePostfixExpression(
  TypePostfixExpressionAst const &other) const -> Ordering {
  // Check the lhs and operator are the same.
  if (*Lhs == *other.Lhs and *TokOp == *other.TokOp) {
    return Ordering::equal;
  }
  return Ordering::less;
}

auto spp::asts::TypePostfixExpressionAst::Equals(
  const ExpressionAst &other) const -> Ordering {
  // Double dispatch to the appropriate equals method.
  return other.EqualsTypePostfixExpression(*this);
}

auto spp::asts::TypePostfixExpressionAst::PosStart() const -> std::size_t {
  // Use the lhs, unless this replaces a written type.
  if (_HasSourceSpan) { return _SpanStart; }
  return Lhs->PosStart();
}

auto spp::asts::TypePostfixExpressionAst::PosEnd() const -> std::size_t {
  // Use the operator, unless this replaces a written type.
  if (_HasSourceSpan) { return _SpanEnd; }
  return TokOp->PosEnd();
}

auto spp::asts::TypePostfixExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto t = MakeUnique<TypePostfixExpressionAst>(
    AstClone(Lhs),
    AstClone(TokOp));
  t->_StampedTypeId = _StampedTypeId;
  CopySourceSpanTo(*t);
  return t;
}

auto spp::asts::TypePostfixExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Lhs);
  SPP_STRING_APPEND(TokOp);
  SPP_STRING_END;
}

auto spp::asts::TypePostfixExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Move through the left-hand-side type.
  Lhs->Stage7_AnalyseSemantics(sm, meta);
  const auto lhs_type_sym = Lhs->InferTypeRef(sm, meta).Symbol;
  const auto lhs_type_scope = lhs_type_sym->LinkedScope;

  // Check there is only 1 target field on the lhs at the
  // highest level. A method's "$" mock is declared once
  // per "sup" block its overloads are written in, and each
  // is given all the overloads, so any one of them will do.
  const auto op_nested = TokOp->As<TypePostfixExpressionOperatorNestedTypeAst>();
  if (not op_nested->Name->IsCompilerGeneratedType()) {
    member_lookup::RaiseIfAmbiguous(
      member_lookup::ClosestScopes(member_lookup::ScopesDeclaringType(*lhs_type_sym->LinkedScope, *op_nested->Name)),
      *op_nested->Name);
  }

  // Ensure the type exists on the "lhs" part.
  const auto _meta_guard = MetaGuard(meta);
  meta->TypeAnalysisTypeScope = lhs_type_scope;
  op_nested->Name->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::TypePostfixExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  return _NestedSymbol(sm, meta)->FqName();
}

auto spp::asts::TypePostfixExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // Read here.
  return TypeRef::Of(*_NestedSymbol(sm, meta), *sm->CurrentScope);
}

auto spp::asts::TypePostfixExpressionAst::_NestedSymbol(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeSymbol* {
  // The nested type the left-hand side's scope declares ("A::B"). The left-hand
  // side analysed again only checks it, once resolved.
  IMPORT_UTILS;
  Lhs->Stage7_AnalyseSemantics(sm, meta);
  const auto lhs_type_sym = Lhs->InferTypeRef(sm, meta).Symbol;
  const auto op_nested = TokOp->As<TypePostfixExpressionOperatorNestedTypeAst>();
  return member_lookup::FindTypeSymbolOrError(
    *lhs_type_sym->LinkedScope, *op_nested->Name);
}

auto spp::asts::TypePostfixExpressionAst::IsNeverType() const noexcept -> bool {
  return false;
}

auto spp::asts::TypePostfixExpressionAst::NsParts() const -> Vec<IdentifierAst const*> {
  // Concatenate the lhs and rhs namespace parts.
  auto parts = Vec<IdentifierAst const*>();
  NsPartsInto(parts);
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::NsParts() -> Vec<IdentifierAst*> {
  // Concatenate the lhs and rhs namespace parts.
  auto parts = Lhs->NsParts();
  parts.append_range(TokOp->NsParts());
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::TypeParts() const -> Vec<TypeIdentifierAst const*> {
  // Concatenate the lhs and rhs type parts.
  auto parts = Vec<TypeIdentifierAst const*>();
  TypePartsInto(parts);
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::TypeParts() -> Vec<TypeIdentifierAst*> {
  // Concatenate the lhs and rhs type parts.
  auto parts = Lhs->TypeParts();
  parts.append_range(TokOp->TypeParts());
  return parts;
}

auto spp::asts::TypePostfixExpressionAst::LastTypePart() const -> TypeIdentifierAst const* {
  // The operator's part (if any) is appended last; otherwise the final part comes from the lhs.
  if (auto const *op_part = std::as_const(*TokOp).LastTypePart()) { return op_part; }
  return std::as_const(*Lhs).LastTypePart();
}

auto spp::asts::TypePostfixExpressionAst::LastTypePart() -> TypeIdentifierAst* {
  if (auto *op_part = TokOp->LastTypePart()) { return op_part; }
  return Lhs->LastTypePart();
}

auto spp::asts::TypePostfixExpressionAst::WithoutConvention() const -> Shared<const TypeAst> {
  return shared_from_this();
}

auto spp::asts::TypePostfixExpressionAst::GetConvention() const -> ConventionAst* {
  // This type AST will never have a convention directly applied to it.
  return nullptr;
}

auto spp::asts::TypePostfixExpressionAst::WithConvention(
  Unique<ConventionAst> &&conv) const -> Shared<TypeAst> {
  if (conv == nullptr) { return const_cast<TypePostfixExpressionAst*>(this)->shared_from_this(); }
  auto borrow_op = MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv));
  auto wrapped = MakeShared<TypeUnaryExpressionAst>(std::move(borrow_op), AstClone(this));
  wrapped->StampTypeId(_StampedTypeId);

  // A type rebuilt in place of a written one keeps pointing at
  // what was written once it is borrowed.
  if (_HasSourceSpan) { CopySourceSpanTo(*wrapped); }
  return wrapped;
}

auto spp::asts::TypePostfixExpressionAst::WithoutGns() const -> Shared<TypeAst> {
  // Use cache if available.
  if (not _CachedWithoutGns) {
    const auto rhs = TokOp->As<TypePostfixExpressionOperatorNestedTypeAst>();
    auto new_rhs = MakeUnique<TypePostfixExpressionOperatorNestedTypeAst>(
      nullptr, shared_cast<TypeIdentifierAst>(rhs->Name->WithoutGns()));
    _CachedWithoutGns = MakeShared<TypePostfixExpressionAst>(AstClone(Lhs), std::move(new_rhs));
  }
  return _CachedWithoutGns;
}

auto spp::asts::TypePostfixExpressionAst::IsCompilerGeneratedType() const -> bool {
  // A method's "$" mock is named through its owner
  // ("main::A::$Method"), so check the nested part.
  return LastTypePart()->IsCompilerGeneratedType();
}

auto spp::asts::TypePostfixExpressionAst::NsPartsInto(
  Vec<IdentifierAst const*> &out) const -> void {
  // Both sides append into the caller's buffer, so a chain of any depth is one allocation.
  std::as_const(*Lhs).NsPartsInto(out);
  std::as_const(*TokOp).NsPartsInto(out);
}

auto spp::asts::TypePostfixExpressionAst::TypePartsInto(
  Vec<TypeIdentifierAst const*> &out) const -> void {
  // Both sides append into the caller's buffer, so a chain of any depth is one allocation.
  std::as_const(*Lhs).TypePartsInto(out);
  std::as_const(*TokOp).TypePartsInto(out);
}

SPP_MOD_END
