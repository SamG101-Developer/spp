#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterSelfAst::FunctionParameterSelfAst(
    decltype(tok_conv) &&tok_conv,
    decltype(var) &&var):
    FunctionParameterAst(std::move(var), nullptr, nullptr),
    tok_conv(std::move(tok_conv)) {
}


auto spp::asts::FunctionParameterSelfAst::pos_end() const -> std::size_t {
    return var->pos_end();
}


spp::asts::FunctionParameterSelfAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterSelfAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_END;
}
