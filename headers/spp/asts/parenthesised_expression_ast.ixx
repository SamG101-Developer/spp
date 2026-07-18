module;
#include <spp/macros.hpp>

export module spp.asts.parenthesised_expression_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ParenthesisedExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::ParenthesisedExpressionAst final : PrimaryExpressionAst {
    /**
     * The @c ( token that indicates the start of a parenthesised expression.
     */
    Unique<TokenAst> TokL;

    /**
     * The expression that is enclosed in parentheses.
     */
    Unique<ExpressionAst> Expr;

    /**
     * The @c ) token that indicates the end of a parenthesised expression.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the ParenthesisedExpressionAst with the arguments matching the members.
     * @param[in] tok_open_paren The @c ( token that indicates the start of a parenthesised expression.
     * @param[in] expr The expression that is enclosed in parentheses.
     * @param[in] tok_close_paren The @c ) token that indicates the end of a parenthesised expression.
     */
    explicit ParenthesisedExpressionAst(
        decltype(TokL) &&tok_open_paren,
        decltype(Expr) &&expr,
        decltype(TokR) &&tok_close_paren);

    ~ParenthesisedExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
