module;
#include <spp/macros.hpp>

export module spp.asts.cmp_statement_ast;
import spp.asts.statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The CmpStatementAst represents a compile time definition statement at either the module or superimposition level. It
 * is analogous to Rust's "const" statement.
 */
SPP_EXP_CLS struct spp::asts::CmpStatementAst final : StatementAst {
    std::vector<std::unique_ptr<AnnotationAst>> annotations;
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<TypeAst> type;
    std::unique_ptr<ExpressionAst> value;

    explicit CmpStatementAst(
        decltype(annotations) &&annotations,
        decltype(name) &&name,
        decltype(type) type,
        decltype(value) &&value);
    ~CmpStatementAst() override;
    auto to_rust() const -> std::string override;
};
