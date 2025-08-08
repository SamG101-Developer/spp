#include <spp/asts/binary_expression_temp_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::BinaryExpressionTempAst::BinaryExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs):
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}
