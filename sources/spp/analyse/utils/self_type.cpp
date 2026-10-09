module spp.analyse.utils.self_type;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import std;

auto spp::analyse::utils::self_type::SubstituteSelf(
  TypeAst const &type, TypeAst const *self, Scope const &scope,
  ScopeManager *const sm, meta::CompilerMetaData *const meta) -> Shared<TypeAst> {
  if (self == nullptr or not type_predicates::DoesTypeNameSelf(type)) { return AstClone(&type); }
  auto bindings = GenericSubst();
  BindSelf(bindings, *self, scope);
  auto out = type_resolution::ReadType(type, ExprSubst::In(scope, std::move(bindings)));
  if (sm == nullptr) { return out; }
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->AllowAbstractType = true;
  out->Stage7_AnalyseSemantics(sm, meta);
  return out;
}

auto spp::analyse::utils::self_type::SubstituteCompSelf(
  ExpressionAst const &value, TypeAst const &with) -> Shared<ExpressionAst> {
  const auto rewrite = [&with](ExpressionAst const &part) { return AstClone(SubstituteCompSelf(part, with)); };
  if (auto const *const type = value.To<TypeAst>(); type != nullptr) { return type->SubstituteSelf(with); }
  if (auto const *const tup = value.To<TupleLiteralAst>(); tup != nullptr) {
    auto elems = Vec<Unique<ExpressionAst>>();
    for (auto const &elem : tup->Elems) { elems.EmplaceBack(rewrite(*elem)); }
    return MakeShared<TupleLiteralAst>(AstClone(tup->TokL), std::move(elems), AstClone(tup->TokR));
  }
  if (auto const *const paren = value.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    return MakeShared<ParenthesisedExpressionAst>(AstClone(paren->TokL), rewrite(*paren->Expr), AstClone(paren->TokR));
  }
  if (auto const *const bin = value.To<BinaryExpressionAst>(); bin != nullptr) {
    if (const auto [lhs, rhs] = bin->Operands(); lhs != nullptr and rhs != nullptr) {
      return MakeShared<BinaryExpressionAst>(rewrite(*lhs), AstClone(bin->TokOp), rewrite(*rhs));
    }
  }
  if (auto const *const pf = value.To<PostfixExpressionAst>(); pf != nullptr) {
    return MakeShared<PostfixExpressionAst>(rewrite(*pf->Lhs), AstClone(pf->Op));
  }
  return AstCloneShared(&value);
}
