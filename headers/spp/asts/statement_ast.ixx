module;
#include <spp/macros.hpp>

export module spp.asts.statement_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct StatementAst;
}


/**
 * The StatementAst class is the base class for all statements in the abstract syntax tree. It is used to represent
 * statements that do not return a value, such as variable declarations and control flow statements.
 */
SPP_EXP_CLS struct spp::asts::StatementAst : Ast {
    explicit StatementAst();
    ~StatementAst() override;
    auto to_rust() const -> std::string override;
};
