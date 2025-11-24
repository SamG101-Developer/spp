#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/is_expression_temp_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::IsExpressionTempAst::IsExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::IsExpressionTempAst::~IsExpressionTempAst() = default;
