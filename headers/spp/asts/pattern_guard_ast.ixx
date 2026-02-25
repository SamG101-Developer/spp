module;
#include <spp/macros.hpp>

export module spp.asts.pattern_guard_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct PatternGuardAst;
}


SPP_EXP_CLS struct spp::asts::PatternGuardAst final : Ast {
    std::unique_ptr<ExpressionAst> expr;

    explicit PatternGuardAst(
        decltype(expr) &&expression);
    ~PatternGuardAst() override;
    auto to_rust() const -> std::string override;
};
