module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_func;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ClosureExpressionAst;
  SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::ClosureExpressionAst final : PrimaryExpressionAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(ClosureExpressionAst);

  /**
   * The optional @c cor keyword. Providing this will turn the closure into a coroutine closure. Otherwise, it will
   * default to @code fun@endcode.
   */
  Unique<TokenAst> Tok;

  /**
   * The parameter and capture group of the closure. This will contain the parameters for the closure, as well as any
   * captured variables from the outer scopes.
   */
  Unique<ClosureExpressionParameterAndCaptureGroupAst> PcGroup;

  /**
   * The optional @c -> token, present exactly when a return type is declared.
   */
  Unique<TokenAst> TokArrow;

  /**
   * The declared return type, or @c nullptr when it is left to be inferred from the body. Declaring one is what lets a
   * closure hand back a variant, that the closure's body only produces a member of, the same way
   * @code let x: Opt[S32] = Some(val=1)@endcode does: inferring from the body gives @c Some[S32] , and a caller
   * holding it as @c Opt[S32] then reads the payload where the discriminant should be. A declared type is what the
   * body is checked against and coerced into.
   */
  Shared<TypeAst> ReturnType;

  /**
   * The body of the closure. This can be a single expression, like @code || 1 + 2@endcode, or an inner scope (type of
   * expression), for more complex closures. A declared return type requires the braced form, because that is the only
   * one a @c ret can be written in.
   */
  Unique<ExpressionAst> Body;

  struct {
    Shared<TypeAst> _OriginalRetType;
  } Source;

  /**
   * Construct the ClosureExpressionAst with the arguments matching the members.
   * @param[in] tok The optional @c cor keyword.
   * @param[in] pc_group The parameter and capture group of the closure.
   * @param[in] tok_arrow The optional @c -> token.
   * @param[in] return_type The declared return type, or @c nullptr to infer it from the body.
   * @param[in] body The body of the closure.
   */
  ClosureExpressionAst(
    decltype(Tok) &&tok,
    decltype(PcGroup) &&pc_group,
    decltype(TokArrow) &&tok_arrow,
    decltype(ReturnType) return_type,
    decltype(Body) &&body);

  ~ClosureExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto GetLlvmFunc() const -> Shared<codegen::LlvmFuncWrapper>;

private:
  /**
   * The inferred return type of the closure. This is determined during semantic analysis and type inference. Must be
   * consistent with each returning value of the closure body.
   */
  Shared<TypeAst> _RetType;

  /**
   * The LLVM function representing the closure. This is generated during code generation stage 11, and is used to
   * call the closure when it is invoked.
   */
  Shared<codegen::LlvmFuncWrapper> _LlvmFunc;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ClosureExpressionAst)
