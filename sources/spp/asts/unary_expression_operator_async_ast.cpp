#include <spp/asts/unary_expression_operator_async_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
    decltype(tok_async) &&tok_async):
    tok_async(std::move(tok_async)) {
}


spp::asts::UnaryExpressionOperatorAsyncAst::~UnaryExpressionOperatorAsyncAst() = default;


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_start() const -> std::size_t {
    return tok_async->pos_start();
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_end() const -> std::size_t {
    return tok_async->pos_end();
}


spp::asts::UnaryExpressionOperatorAsyncAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_async);
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_async);
    SPP_PRINT_END;
}
