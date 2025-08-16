#include <spp/asts/function_parameter_variadic_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterVariadicAst::FunctionParameterVariadicAst(
    decltype(tok_ellipsis) &&tok_ellipsis,
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type) :
    FunctionParameterAst(std::move(var), std::move(tok_colon), std::move(type)),
    tok_ellipsis(std::move(tok_ellipsis)) {
}


auto spp::asts::FunctionParameterVariadicAst::pos_start() const -> std::size_t {
    return tok_ellipsis->pos_start();
}


auto spp::asts::FunctionParameterVariadicAst::pos_end() const -> std::size_t {
    return tok_ellipsis->pos_end();
}


auto spp::asts::FunctionParameterVariadicAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionParameterVariadicAst>(
        ast_clone(tok_ellipsis),
        ast_clone(var),
        ast_clone(tok_colon),
        ast_clone(type));
}


spp::asts::FunctionParameterVariadicAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterVariadicAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_END;
}
