module;
#include <spp/macros.hpp>

export module spp.asts.tuple_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TupleLiteralAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::TupleLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(TupleLiteralAst);

  /**
   * The left parenthesis token that represents the start of the tuple literal.
   */
  Unique<TokenAst> TokL;

  /**
   * The elements of the tuple literal. This is a list of expressions that are contained within the tuple.
   */
  Vec<Unique<ExpressionAst>> Elems;

  /**
   * The right parenthesis token that represents the end of the tuple literal.
   */
  Unique<TokenAst> TokR;

  /**
   * Construct the TupleLiteralAst with the arguments matching the members.
   * @param tok_l The left parenthesis token.
   * @param elements The elements of the tuple literal.
   * @param tok_r The right parenthesis token.
   */
  TupleLiteralAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elements,
    decltype(TokR) &&tok_r);

  ~TupleLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsTupleLiteral(TupleLiteralAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TupleLiteralAst)
