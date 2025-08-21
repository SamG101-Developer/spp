#pragma once

#include <spp/asts/_fwd.hpp>

namespace spp::analyse::scopes {
    class ScopeManager;
}


namespace spp::analyse::utils::bin_utils {
    auto fix_associativity(
        asts::BinaryExpressionAst const &bin_expr)
        -> std::unique_ptr<asts::BinaryExpressionAst>;

    auto combine_comp_ops(
        asts::BinaryExpressionAst const &bin_expr)
        -> std::unique_ptr<asts::BinaryExpressionAst>;

    auto convert_bin_expr_to_function_call(
        asts::BinaryExpressionAst const &bin_expr,
        scopes::ScopeManager const &sm)
        -> std::unique_ptr<asts::PostfixExpressionAst>;

    auto convert_is_expr_to_function_call(
        asts::IsExpressionAst const &is_expr,
        scopes::ScopeManager const &sm)
        -> std::unique_ptr<asts::PostfixExpressionAst>;
}
