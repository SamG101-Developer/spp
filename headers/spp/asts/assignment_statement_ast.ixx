module;
#include <spp/macros.hpp>

export module spp.asts.assignment_statement_ast;
import spp.asts.statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AssignmentStatementAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * An assignment statement allows for one value to be moved or copied into another variable or property (or multiple
 * values into multiple variables or properties at once). Only symbolic variables and properties can be assigned to.
 *
 * @note Compound assignment expressions, such as @c += or @c -=, are classified under @c BinaryExpressionAst nodes,
 * because they share function mapping logic with binary expressions, and can only have 1 left-hand-side expression
 * anyway. Some checks are copied from this class (symbolic target, etc.).
 */
SPP_EXP_CLS struct spp::asts::AssignmentStatementAst final : StatementAst {
    std::vector<std::unique_ptr<ExpressionAst>> lhs;
    std::vector<std::unique_ptr<ExpressionAst>> rhs;

    explicit AssignmentStatementAst(
        decltype(lhs) &&lhs,
        decltype(rhs) &&rhs);
    ~AssignmentStatementAst() override;
    auto to_rust() const -> std::string override;
};
