module;
#include <spp/macros.hpp>

export module spp.asts.use_statement_ast;
import spp.asts.statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct UseStatementAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The UseStatementAst reduces a fully qualified tpy into the current scope, making the symbol accessible without the
 * associated namespace. It is internal mapped to a TypeStatementAst: @code use std::Str@endcode is equivalent to
 * @code type Str = std::Str@endcode.
 */
SPP_EXP_CLS struct spp::asts::UseStatementAst final : StatementAst {
    std::vector<std::unique_ptr<AnnotationAst>> annotations;
    std::unique_ptr<TypeAst> old_type;

    explicit UseStatementAst(
        decltype(annotations) &&annotations,
        decltype(old_type) old_type);
    ~UseStatementAst() override;
    auto to_rust() const -> std::string override;
};
