#include <spp/asts/type_binary_expression_temp_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::TypeBinaryExpressionTempAst::TypeBinaryExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs):
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::TypeBinaryExpressionTempAst::~TypeBinaryExpressionTempAst() = default;
