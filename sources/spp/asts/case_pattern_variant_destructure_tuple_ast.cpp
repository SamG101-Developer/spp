module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::CasePatternVariantDestructureTupleAst::CasePatternVariantDestructureTupleAst(
  decltype(TokL) &&tok_l,
  decltype(Elems) &&elems,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Elems(std::move(elems)),
  TokR(std::move(tok_r)) {
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokL, SppTokenType::TK_LEFT_PARENTHESIS, "(");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokR, SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}

spp::asts::CasePatternVariantDestructureTupleAst::~CasePatternVariantDestructureTupleAst() = default;

auto spp::asts::CasePatternVariantDestructureTupleAst::PosStart() const -> std::size_t {
  // Use the "(" token.
  return TokL->PosStart();
}

auto spp::asts::CasePatternVariantDestructureTupleAst::PosEnd() const -> std::size_t {
  // Use the ")" token.
  return TokR->PosEnd();
}

auto spp::asts::CasePatternVariantDestructureTupleAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto c = MakeUnique<CasePatternVariantDestructureTupleAst>(
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
  c->_MappedLet = AstClone(_MappedLet);
  return c;
}

auto spp::asts::CasePatternVariantDestructureTupleAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokL);
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

auto spp::asts::CasePatternVariantDestructureTupleAst::BindsByMove() const -> bool {
  // A destructure binds if any of its elements does. An
  // empty one, or one made only of skips, is a shape test
  // and takes nothing.
  return genex::any_of(Elems, [](auto const &elem) { return elem->BindsByMove(); });
}

auto spp::asts::CasePatternVariantDestructureTupleAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Map the pattern to a "let" over the condition, introducing
  // its bindings.
  AnalyseDestructure(
    meta->CaseCondition, Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto spp::asts::CasePatternVariantDestructureTupleAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward memory checking to the mapped let statement.
  _MappedLet->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::CasePatternVariantDestructureTupleAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Run the mapped "let"; the "case"'s plan tests the literals.
  return CodeGenDestructure(sm, meta, ctx);
}

auto spp::asts::CasePatternVariantDestructureTupleAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Resolve the mapped "let"; the "case"'s plan tests the literals.
  EvaluateCompTimeDestructure(sm, meta);
}

auto spp::asts::CasePatternVariantDestructureTupleAst::ConvToVar(
  CompilerMetaData *meta) -> Unique<LocalVariableAst> {
  // Recursively map the elements to their local variable
  // counterparts.
  auto mapped_elems = Elems
    | genex::views::transform([&](auto const &x) { return x->ConvToVar(meta); })
    | genex::to<Vec>();

  // Create the final local variable wrapping, tag it and return it.
  auto var = MakeUnique<LocalVariableDestructureTupleAst>(
    AstClone(TokL), std::move(mapped_elems), AstClone(TokR));
  var->MarkFromCasePattern();
  return var;
}

SPP_MOD_END
