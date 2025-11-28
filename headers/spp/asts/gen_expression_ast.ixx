module;
#include <spp/macros.hpp>

export module spp.asts.gen_expression_ast;
import spp.asts._fwd;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenExpressionAst;
}


/**
 * The GenExpressionAst represents a value being yielded out of a coroutine. A convention can be applied to the value,
 * to create foundational structures like iterators.
 */
SPP_EXP_CLS struct spp::asts::GenExpressionAst final : PrimaryExpressionAst {
private:
    std::shared_ptr<TypeAst> m_gen_type;

public:
    /**
     * The token that represents a generation point. This is the @c gen keyword in the source code, which indicates that
     * the coroutine is suspending its execution and yielding a value.
     */
    std::unique_ptr<TokenAst> tok_gen;

    /**
     * An optional convention that can be applied to the value being yielded. This allows for additional behaviour to be
     * attached to the value, such as making it an iterator.
     */
    std::unique_ptr<ConventionAst> conv;

    /**
     * The expression that is being yielded out of the coroutine. This is the value that will be returned when the
     * coroutine is resumed.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the GenExpressionAst with the arguments matching the members.
     * @param tok_gen The token that represents the @c gen keyword in the source code.
     * @param conv The optional convention that can be applied to the value being yielded.
     * @param expr The expression that is being yielded out of the coroutine.
     */
    GenExpressionAst(
        decltype(tok_gen) &&tok_gen,
        decltype(conv) &&conv,
        decltype(expr) &&expr);

    ~GenExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
