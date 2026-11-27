module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.utils.case_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::CasePatternVariantLiteralAst::CasePatternVariantLiteralAst(
  decltype(Literal) &&literal) :
  Literal(std::move(literal)) {
}

spp::asts::CasePatternVariantLiteralAst::~CasePatternVariantLiteralAst() = default;

auto spp::asts::CasePatternVariantLiteralAst::PosStart() const -> std::size_t {
  // Use the literal.
  return Literal->PosStart();
}

auto spp::asts::CasePatternVariantLiteralAst::PosEnd() const -> std::size_t {
  // Use the literal.
  return Literal->PosEnd();
}

auto spp::asts::CasePatternVariantLiteralAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<CasePatternVariantLiteralAst>(
    AstClone(Literal));
}

auto spp::asts::CasePatternVariantLiteralAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Literal);
  SPP_STRING_END;
}

auto spp::asts::CasePatternVariantLiteralAst::ConvToVar(
  CompilerMetaData *) -> Unique<LocalVariableAst> {
  // A literal is tested by the "case"'s plan, not bound: in the
  // pattern's "let" it is a skip ("_"), which takes nothing, so a
  // pattern of literals only tests the shape of its value.
  auto var = MakeUnique<LocalVariableDestructureSkipSingleArgumentAst>(
    MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::TK_UNDERSCORE, "_"));
  var->MarkFromCasePattern();
  return var;
}

SPP_MOD_END
