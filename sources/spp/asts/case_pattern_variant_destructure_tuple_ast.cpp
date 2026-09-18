module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
CasePatternVariantDestructureTupleAst::CasePatternVariantDestructureTupleAst(
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

CasePatternVariantDestructureTupleAst::~CasePatternVariantDestructureTupleAst() = default;

auto CasePatternVariantDestructureTupleAst::PosStart() const -> std::size_t {
  // Use the "(" token.
  return TokL->PosStart();
}

auto CasePatternVariantDestructureTupleAst::PosEnd() const -> std::size_t {
  // Use the ")" token.
  return TokR->PosEnd();
}

auto CasePatternVariantDestructureTupleAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto c = MakeUnique<CasePatternVariantDestructureTupleAst>(
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
  c->_MappedLet = AstClone(_MappedLet);
  return c;
}

auto CasePatternVariantDestructureTupleAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokL);
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

auto CasePatternVariantDestructureTupleAst::BindsByMove() const -> bool {
  // A destructure binds if any of its elements does. An
  // empty one, or one made only of skips, is a shape test
  // and takes nothing.
  return genex::any_of(Elems, [](auto const &elem) { return elem->BindsByMove(); });
}

auto CasePatternVariantDestructureTupleAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Map the pattern to a "let" over the condition, introducing its bindings.
  AnalyseDestructure(
    meta->CaseCondition, Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto CasePatternVariantDestructureTupleAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward memory checking to the mapped let statement.
  _MappedLet->Stage8_CheckMemory(sm, meta);
}

auto CasePatternVariantDestructureTupleAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Match when every element does.
  ResolveDestructure(Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto CasePatternVariantDestructureTupleAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm;

  // Run the codegen on the transformed "let" ast to introduce
  // symbols into the llvm function.
  if (_MappedLet != nullptr) {
    const auto _meta_guard = MetaGuard(meta);
    meta->LetStatementPrecomputedValue = meta->LlvmCaseCondition;
    _MappedLet->Stage11_CodeGen(sm, meta, ctx);
  }

  // Combine all the generated transforms into a single "AND"ed
  // expression.
  auto llvm_transforms = CreateAndAnalysePatternEqFuncsLlvm(
    Elems | genex::views::ptr | genex::to<Vec>(), sm, meta, ctx);

  const auto AND = [&ctx](auto a, auto b) { return ctx->Builder.CreateAnd(a, b); };
  const auto llvm_master_transform = llvm_transforms.IsEmpty()
    ? llvm::cast<llvm::Value>(llvm::ConstantInt::getTrue(*ctx->Context))
    : genex::fold_left_first(llvm_transforms, std::move(AND));

  // Return the combined expression back to the branch who owns
  // this pattern.
  return llvm_master_transform;
}

auto CasePatternVariantDestructureTupleAst::ConvToVar(
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
