#pragma once
#include <spp/asts/expression_ast.hpp>


struct spp::asts::PostfixExpressionAst final : ExpressionAst {
    /**
     * The left-hand side expression of the postfix expression. This is the base expression on which the postfix operation
     * is applied.
     */
    std::unique_ptr<ExpressionAst> lhs;

    /**
     * The operator token that represents the postfix operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<PostfixExpressionOperatorAst> op;

    /**
     * Construct the PostfixExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side expression of the postfix expression.
     * @param[in] op The operator token that represents the postfix operation.
     */
    PostfixExpressionAst(
        decltype(lhs) &&lhs,
        decltype(op) &&op);

    ~PostfixExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, mixins::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(analyse::scopes::ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
