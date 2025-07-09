#include <spp/asts/expression_ast.hpp>
#include <spp/asts/function_parameter_optional_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterOptionalAst::FunctionParameterOptionalAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val):
    FunctionParameterAst(std::move(var), std::move(tok_colon), std::move(type)),
    tok_assign(std::move(tok_assign)),
    default_val(std::move(default_val)) {
}


auto spp::asts::FunctionParameterOptionalAst::pos_start() const -> std::size_t {
    return default_val->pos_start();
}


auto spp::asts::FunctionParameterOptionalAst::pos_end() const -> std::size_t {
    return default_val->pos_end();
}


spp::asts::FunctionParameterOptionalAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterOptionalAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(default_val);
    SPP_PRINT_END;
}
