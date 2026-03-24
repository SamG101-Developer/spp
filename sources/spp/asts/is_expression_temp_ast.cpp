module;
#include <spp/macros.hpp>

module spp.asts.is_expression_temp_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;


SPP_MOD_BEGIN
spp::asts::IsExpressionTempAst::IsExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::IsExpressionTempAst::~IsExpressionTempAst() = default;

SPP_MOD_END
