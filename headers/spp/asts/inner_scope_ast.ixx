module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL_TEMPLATED(InnerScopeAst) {
  SPP_EXP_CLS struct TokenAst;
}

/// An inner scope for top level structure implementations
/// (classes, functions, etc.). Bodies of expression asts, such
/// as the "case" or "loop" structures, use the
/// InnerScopeExpressionAst class instead, which supports type
/// inference etc.
SPP_EXP_CLS template <typename T>
struct spp::asts::InnerScopeAst : Ast {
  SPP_AST_KEY_FUNCTIONS(InnerScopeAst);

  /// The "{" token that starts the inner scope.
  Unique<TokenAst> TokL;

  /// The members in the inner scope. They are all the "T"
  /// type or a derived type, so different kinds of asts can
  /// share one inner scope, as long as they derive from a
  /// common base class.
  Vec<T> Members;

  /// The "}" token that ends the inner scope.
  Unique<TokenAst> TokR;

  static auto NewEmpty() -> Unique<InnerScopeAst>;

  InnerScopeAst(
    decltype(TokL) &&tok_l,
    decltype(Members) &&members,
    decltype(TokR) &&tok_r);

  ~InnerScopeAst() override;

  auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto FinalMember() const -> Ast*;

protected:
  InnerScopeAst();
};
