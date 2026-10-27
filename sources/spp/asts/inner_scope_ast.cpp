module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.inner_scope_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.class_member_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.sup_member_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.lsp.resolution_index;
import spp.utils.ice;
import genex;

SPP_MOD_BEGIN
template <typename T>
auto InnerScopeAst<T>::NewEmpty() -> Unique<InnerScopeAst> {
  return MakeUnique<InnerScopeAst>(
    nullptr, decltype(Members)(), nullptr);
}

template <typename T>
InnerScopeAst<T>::InnerScopeAst() :
  TokL(nullptr),
  Members(),
  TokR(nullptr) {
}

template <typename T>
InnerScopeAst<T>::InnerScopeAst(
  decltype(TokL) &&tok_l,
  decltype(Members) &&members,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Members(std::move(members)),
  TokR(std::move(tok_r)) {
}

template <typename T>
InnerScopeAst<T>::~InnerScopeAst() = default;

template <typename T>
auto InnerScopeAst<T>::PosStart() const -> std::size_t {
  // Use the "{" token.
  return TokL->PosStart();
}

template <typename T>
auto InnerScopeAst<T>::PosEnd() const -> std::size_t {
  // Use the "}" token.
  return TokR->PosEnd();
}

template <typename T>
auto InnerScopeAst<T>::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<InnerScopeAst>(
    AstClone(TokL), AstCloneVec(Members), AstClone(TokR));
}

template <typename T>
auto InnerScopeAst<T>::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("{").append(not Members.empty() ? "\n" : "");
  SPP_STRING_EXTEND(Members, "\n");
  SPP_STRING_APPEND_RAW(not Members.empty() ? "\n" : "");
  SPP_STRING_APPEND_RAW("}");
  SPP_STRING_END;
}

template <typename T>
auto InnerScopeAst<T>::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Add all the expressions/statements into the current scope.
  sm->MoveToNextScope();
  // SPP_ASSERT(sm->CurrentScope == _Scope);

  for (auto const &m : Members) { m->Stage11_CodeGen(sm, meta, ctx); }

  // Need to add a "return void" if the final member isn't a
  // RetStatementAst.
  if (Members.empty() or Members.back()->template To<RetStatementAst>() == nullptr) {
    ctx->Builder.CreateRetVoid();
  }

  // Exit the scope.
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

template <typename T>
auto InnerScopeAst<T>::FinalMember() const -> Ast* {
  return Members.empty() ? TokR->To<Ast>() : Members.back()->template To<Ast>();
}

CLANG_STRICT_TEMPLATE_FULLY_QUALIFIED_INSTANTIATION
template struct spp::asts::InnerScopeAst<spp::Unique<Ast>>;
SPP_MOD_END
