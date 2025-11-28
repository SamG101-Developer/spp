module;
#include <spp/macros.hpp>

export module spp.asts.parenthesised_expression_ast;
import spp.asts._fwd;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ParenthesisedExpressionAst;
}


SPP_EXP_CLS struct spp::asts::ParenthesisedExpressionAst final : PrimaryExpressionAst {
    /**
     * The @c ( token that indicates the start of a parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_open_paren;

    /**
     * The expression that is enclosed in parentheses.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * The @c ) token that indicates the end of a parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_close_paren;

    /**
     * Construct the ParenthesisedExpressionAst with the arguments matching the members.
     * @param[in] tok_open_paren The @c ( token that indicates the start of a parenthesised expression.
     * @param[in] expr The expression that is enclosed in parentheses.
     * @param[in] tok_close_paren The @c ) token that indicates the end of a parenthesised expression.
     */
    explicit ParenthesisedExpressionAst(
        decltype(tok_open_paren) &&tok_open_paren,
        decltype(expr) &&expr,
        decltype(tok_close_paren) &&tok_close_paren);

    ~ParenthesisedExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
