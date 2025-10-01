#include <spp/asts/function_parameter_required_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterRequiredAst::FunctionParameterRequiredAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type) :
    FunctionParameterAst(std::move(var), std::move(tok_colon), std::move(type)) {
}


spp::asts::FunctionParameterRequiredAst::~FunctionParameterRequiredAst() = default;


auto spp::asts::FunctionParameterRequiredAst::pos_start() const
    -> std::size_t {
    return var->pos_start();
}


auto spp::asts::FunctionParameterRequiredAst::pos_end() const
    -> std::size_t {
    return type->pos_end();
}


auto spp::asts::FunctionParameterRequiredAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionParameterRequiredAst>(
        ast_clone(var),
        ast_clone(tok_colon),
        ast_clone(type));
}


spp::asts::FunctionParameterRequiredAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterRequiredAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_END;
}
