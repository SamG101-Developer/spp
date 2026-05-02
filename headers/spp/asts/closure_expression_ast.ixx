module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_ast;
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
     * The body of the closure. This can be a single expression, like @code || 1 + 2@endcode, or an inner scope (type of
     * expression), for more complex closures.
     */
    Unique<ExpressionAst> Body;

    /**
     * Construct the ClosureExpressionAst with the arguments matching the members.
     * @param[in] tok The optional @c cor keyword.
     * @param[in] pc_group The parameter and capture group of the closure.
     * @param[in] body The body of the closure.
     */
    ClosureExpressionAst(
        decltype(Tok) &&tok,
        decltype(PcGroup) &&pc_group,
        decltype(Body) &&body);

    ~ClosureExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

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
