module;
#include <spp/macros.hpp>

export module spp.asts.loop_else_statement_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::LoopElseStatementAst final : Ast, mixins::TypeInferrableAst {
    /**
     * The @c else keyword that indicates this is an else statement for the loop.
     */
    Unique<TokenAst> TokElse;

    /**
     * The body of the else statement. This is a block of statements that will be executed if the loop condition is
     * immediately false, or the iterable is already exhausted (no loops take place).
     */
    Unique<InnerScopeExpressionAst> Body;

    /**
     * Construct the LoopElseStatementAst with the arguments matching the members.
     * @param[in] tok_else The @c else keyword that indicates this is an else statement for the loop.
     * @param[in] body The body of the else statement.
     */
    LoopElseStatementAst(
        decltype(TokElse) &&tok_else,
        decltype(Body) &&body);

    ~LoopElseStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;
};
