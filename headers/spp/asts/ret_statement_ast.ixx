module;
#include <spp/macros.hpp>

export module spp.asts.ret_statement_ast;
import spp.asts.statement_ast;

import std;


SPP_EXP struct spp::asts::RetStatementAst final : StatementAst {
private:
    std::shared_ptr<TypeAst> m_ret_type;

public:
    /**
     * The @c ret token that starts this statement.
     */
    std::unique_ptr<TokenAst> tok_ret;

    /**
     * The optional value that is being returned from the function. This is the expression that will be evaluated and
     * returned.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the RetStatementAst with the arguments matching the members.
     * @param tok_ret The @c return token that starts this statement.
     * @param val The optional value that is being returned from the function.
     */
    RetStatementAst(
        decltype(tok_ret) &&tok_ret,
        decltype(expr) &&val);

    ~RetStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
