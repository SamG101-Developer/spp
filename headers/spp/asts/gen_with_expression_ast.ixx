module;
#include <spp/macros.hpp>

export module spp.asts.gen_with_expression_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenWithExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::GenWithExpressionAst final : PrimaryExpressionAst {
private:
    std::shared_ptr<TypeAst> m_generator_type;

public:
    /**
     * The token that represents a generation point. This is the @c gen keyword in the source code, which indicates that
     * the coroutine is suspending its execution and yielding a value.
     */
    std::unique_ptr<TokenAst> tok_gen;

    /**
     * The token that represents the @c with keyword in the source code. This indicates that the expression is being
     * generated with a specific context.
     */
    std::unique_ptr<TokenAst> tok_with;

    /**
     * The expression that is being generated with the context. This is the value that will be used in the generation.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the GenWithExpressionAst with the arguments matching the members.
     * @param tok_gen The token that represents the @c gen keyword in the source code.
     * @param tok_with The token that represents the @c with keyword in the source code.
     * @param expr The expression that is being generated with the context.
     */
    GenWithExpressionAst(
        decltype(tok_gen) &&tok_gen,
        decltype(tok_with) &&tok_with,
        decltype(expr) &&expr);

    ~GenWithExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
