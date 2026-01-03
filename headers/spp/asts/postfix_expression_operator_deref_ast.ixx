module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorDerefAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorDerefAst final : PostfixExpressionOperatorAst {
    /**
     * The @c * token that indicates a dereference operation. This is used to extract a copyable value from a borrow.
     */
    std::unique_ptr<TokenAst> tok_deref;

    /**
     * Construct the PostfixExpressionOperatorDerefAst with the arguments matching the members.
     * @param tok_deref The @c * token that indicates a dereference operation.
     */
    explicit PostfixExpressionOperatorDerefAst(
        decltype(tok_deref) &&tok_deref);

    ~PostfixExpressionOperatorDerefAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
