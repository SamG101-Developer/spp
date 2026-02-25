module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_uninitialized_ast;
import spp.asts.let_statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LetStatementUninitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LetStatementUninitializedAst final : LetStatementAst {
    std::unique_ptr<LocalVariableAst> var;
    std::unique_ptr<TypeAst> type;

    explicit LetStatementUninitializedAst(
        decltype(var) &&var,
        decltype(type) type);
    ~LetStatementUninitializedAst() override;
    auto to_rust() const -> std::string override;
};
