#ifndef RET_STATEMENT_AST_HPP
#define RET_STATEMENT_AST_HPP

#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::RetStatementAst final : StatementAst {
    SPP_AST_KEY_FUNCTIONS;

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

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //RET_STATEMENT_AST_HPP
