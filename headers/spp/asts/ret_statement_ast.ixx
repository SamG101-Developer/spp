module;
#include <spp/macros.hpp>

export module spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct RetStatementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::RetStatementAst final : StatementAst {
    /**
     * The @c ret token that starts this statement.
     */
    Unique<TokenAst> TokRet;

    /**
     * The optional value that is being returned from the function. This is the expression that will be evaluated and
     * returned.
     */
    Unique<ExpressionAst> Expr;

    /**
     * Construct the RetStatementAst with the arguments matching the members.
     * @param tok_ret The @c return token that starts this statement.
     * @param val The optional value that is being returned from the function.
     */
    RetStatementAst(
        decltype(TokRet) &&tok_ret,
        decltype(Expr) &&val);

    ~RetStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

private:
    Shared<TypeAst> _RetType;
};
