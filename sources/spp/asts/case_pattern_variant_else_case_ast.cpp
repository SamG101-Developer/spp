module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
CasePatternVariantElseCaseAst::CasePatternVariantElseCaseAst(
  decltype(TokElse) &&tok_else,
  decltype(CaseExpr) &&case_expr) :
  TokElse(std::move(tok_else)),
  CaseExpr(std::move(case_expr)) {
}

CasePatternVariantElseCaseAst::~CasePatternVariantElseCaseAst() = default;

auto CasePatternVariantElseCaseAst::PosStart() const -> std::size_t {
  // Use the "else" token.
  return TokElse->PosStart();
}

auto CasePatternVariantElseCaseAst::PosEnd() const -> std::size_t {
  // Use the [case expression]'s condition.
  return CaseExpr->Cond->PosEnd();
}

auto CasePatternVariantElseCaseAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<CasePatternVariantElseCaseAst>(
    AstClone(TokElse),
    AstClone(CaseExpr));
}

auto CasePatternVariantElseCaseAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokElse);
  SPP_STRING_APPEND(CaseExpr);
  SPP_STRING_END;
}

SPP_MOD_END
