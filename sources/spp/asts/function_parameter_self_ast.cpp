#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterSelfAst::FunctionParameterSelfAst(
    decltype(tok_conv) &&tok_conv,
    decltype(var) &&var) :
    FunctionParameterAst(std::move(var), nullptr, nullptr),
    tok_conv(std::move(tok_conv)) {
}


spp::asts::FunctionParameterSelfAst::~FunctionParameterSelfAst() = default;


auto spp::asts::FunctionParameterSelfAst::pos_start() const -> std::size_t {
    return tok_conv->pos_start();
}


auto spp::asts::FunctionParameterSelfAst::pos_end() const -> std::size_t {
    return var->pos_end();
}


auto spp::asts::FunctionParameterSelfAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionParameterSelfAst>(
        ast_clone(*tok_conv),
        ast_clone(*var));
}


spp::asts::FunctionParameterSelfAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterSelfAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_END;
}


auto spp::asts::FunctionParameterSelfAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Perform default analysis steps.
    FunctionParameterAst::stage_7_analyse_semantics(sm, meta);

    // Special mutability rules for the "self" parameter.
    const auto sym = sm->current_scope->get_var_symbol(*var->extract_name());
    sym->is_mutable = ast_cast<LocalVariableSingleIdentifierAst>(var.get())->tok_mut != nullptr
        or ast_cast<ConventionMutAst>(tok_conv.get()) != nullptr;;
}
