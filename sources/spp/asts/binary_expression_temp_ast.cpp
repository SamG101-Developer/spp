module;
#include <spp/macros.hpp>

module spp.asts.binary_expression_temp_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;


SPP_MOD_BEGIN
spp::asts::BinaryExpressionTempAst::BinaryExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::BinaryExpressionTempAst::~BinaryExpressionTempAst() = default;

SPP_MOD_END
