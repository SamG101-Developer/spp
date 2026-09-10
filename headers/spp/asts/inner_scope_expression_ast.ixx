module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_expression_ast;
import spp.asts.ast_kind;
import spp.asts.inner_scope_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct InnerScopeExpressionAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::InnerScopeExpressionAst : PrimaryExpressionAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(InnerScopeExpressionAst);

  /**
   * The @c { token that represents the start of the inner scope. This is used to indicate the beginning of the scope
   * and is typically followed by a list of members or statements that belong to this scope.
   */
  Unique<TokenAst> TokL;

  /**
   * The list of members in the inner scope. They are all the @c T type, or a derived type. This allows for a flexible
   * structure where different types of ASTs can be included in the same inner scope, as long as they derive from
   * a common base class.
   */
  Vec<Unique<StatementAst>> Members;

  /**
   * The @c } token that represents the end of the inner scope. This is used to indicate the end of the scope after
   * all the members or statements have been defined.
   */
  Unique<TokenAst> TokR;

  static auto NewEmpty()
    -> Unique<InnerScopeExpressionAst>;

  InnerScopeExpressionAst(
    decltype(TokL) &&tok_l,
    decltype(Members) &&members,
    decltype(TokR) &&tok_r);

  ~InnerScopeExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  /**
   * Whether the value of the final statement in this scope goes nowhere. A block hands its final statement's value to
   * whoever wrote the block, so normally the answer is no and that statement is exempt from the discarded-value check.
   * A function body is the exception: S++ returns through @c ret , so a body's final statement is discarded like any
   * other.
   * @return Whether the final member is in discard position.
   */
  SPP_ATTR_NODISCARD virtual auto DiscardsFinalMember() const -> bool;

  SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

  SPP_ATTR_NODISCARD auto FinalMember() const -> Ast*;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::InnerScopeExpressionAst)
