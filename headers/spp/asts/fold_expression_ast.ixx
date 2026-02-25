module;
#include <spp/macros.hpp>

export module spp.asts.fold_expression_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FoldExpressionAst;
}


SPP_EXP_CLS struct spp::asts::FoldExpressionAst final : PrimaryExpressionAst {
    explicit FoldExpressionAst();
    ~FoldExpressionAst() override;
    auto to_rust() const -> std::string override;
};
