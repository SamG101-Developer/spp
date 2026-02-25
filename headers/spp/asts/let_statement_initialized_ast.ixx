module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_initialized_ast;
import spp.asts.let_statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LetStatementInitializedAst final : LetStatementAst {
    std::unique_ptr<LocalVariableAst> var;
    std::unique_ptr<TypeAst> type;
    std::unique_ptr<ExpressionAst> val;

    explicit LetStatementInitializedAst(
        decltype(var) &&var,
        decltype(type) type,
        decltype(val) &&val);
    ~LetStatementInitializedAst() override;
    auto to_rust() const -> std::string override;
};
