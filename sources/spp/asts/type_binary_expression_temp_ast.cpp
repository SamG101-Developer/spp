module spp.asts.type_binary_expression_temp_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;


spp::asts::TypeBinaryExpressionTempAst::TypeBinaryExpressionTempAst(
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::TypeBinaryExpressionTempAst::~TypeBinaryExpressionTempAst() = default;
