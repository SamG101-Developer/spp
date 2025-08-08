#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/is_expression_temp_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::IsExpressionTempAst::IsExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs):
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::IsExpressionTempAst::~IsExpressionTempAst() = default;
