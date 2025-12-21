module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionAst;
    SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionAst final : PrimaryExpressionAst {
protected:
    std::shared_ptr<TypeAst> m_ret_type;

public:
    llvm::Function *llvm_func;

    /**
     * The optional @c cor keyword. Providing this will turn the closure into a coroutine closure. Otherwise, it will
     * default to @code fun@endcode.
     */
    std::unique_ptr<TokenAst> tok;

    /**
     * The parameter and capture group of the closure. This will contain the parameters for the closure, as well as any
     * captured variables from the outer scopes.
     */
    std::unique_ptr<ClosureExpressionParameterAndCaptureGroupAst> pc_group;

    /**
     * The body of the closure. This can be a single expression, like @code || 1 + 2@endcode, or an inner scope (type of
     * expression), for more complex closures.
     */
    std::unique_ptr<ExpressionAst> body;

    /**
     * Construct the ClosureExpressionAst with the arguments matching the members.
     * @param[in] tok The optional @c cor keyword.
     * @param[in] pc_group The parameter and capture group of the closure.
     * @param[in] body The body of the closure.
     */
    ClosureExpressionAst(
        decltype(tok) &&tok,
        decltype(pc_group) &&pc_group,
        decltype(body) &&body);

    ~ClosureExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

private:
    std::shared_ptr<TypeAst> m_return_type;
};
