module;
#include <spp/macros.hpp>

export module spp.asts.loop_else_statement_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LoopElseStatementAst final : virtual Ast, mixins::TypeInferrableAst {
    /**
     * The @c else keyword that indicates this is an else statement for the loop.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * The body of the else statement. This is a block of statements that will be executed if the loop condition is
     * immediately false, or the iterable is already exhausted (no loops take place).
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * Construct the LoopElseStatementAst with the arguments matching the members.
     * @param[in] tok_else The @c else keyword that indicates this is an else statement for the loop.
     * @param[in] body The body of the else statement.
     */
    LoopElseStatementAst(
        decltype(tok_else) &&tok_else,
        decltype(body) &&body);

    ~LoopElseStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm , CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
