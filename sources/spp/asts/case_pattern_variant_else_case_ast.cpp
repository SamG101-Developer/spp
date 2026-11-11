module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::CasePatternVariantElseCaseAst::CasePatternVariantElseCaseAst(
  decltype(TokElse) &&tok_else,
  decltype(CaseExpr) &&case_expr) :
  TokElse(std::move(tok_else)),
  CaseExpr(std::move(case_expr)) {
}

spp::asts::CasePatternVariantElseCaseAst::~CasePatternVariantElseCaseAst() = default;

auto spp::asts::CasePatternVariantElseCaseAst::PosStart() const -> std::size_t {
  // Use the "else" token.
  return TokElse->PosStart();
}

auto spp::asts::CasePatternVariantElseCaseAst::PosEnd() const -> std::size_t {
  // Use the [case expression]'s condition.
  return CaseExpr->Cond->PosEnd();
}

auto spp::asts::CasePatternVariantElseCaseAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<CasePatternVariantElseCaseAst>(
    AstClone(TokElse),
    AstClone(CaseExpr));
}

auto spp::asts::CasePatternVariantElseCaseAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokElse);
  SPP_STRING_APPEND(CaseExpr);
  SPP_STRING_END;
}

SPP_MOD_END
