module;
#include <spp/macros.hpp>

module spp.asts.function_parameter_self_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;


spp::asts::FunctionParameterSelfAst::FunctionParameterSelfAst(
    decltype(conv) &&conv,
    decltype(var) &&var) :
    FunctionParameterAst(std::move(var), nullptr, nullptr, utils::OrderableTag::SELF_PARAM),
    conv(std::move(conv)) {
    type = generate::common_types::self_type(pos_start());
}


spp::asts::FunctionParameterSelfAst::~FunctionParameterSelfAst() = default;


auto spp::asts::FunctionParameterSelfAst::pos_start() const
    -> std::size_t {
    return conv != nullptr ? conv->pos_start() : var->pos_start();
}


auto spp::asts::FunctionParameterSelfAst::pos_end() const
    -> std::size_t {
    return var->pos_end();
}


auto spp::asts::FunctionParameterSelfAst::clone() const
    -> std::unique_ptr<Ast> {
    auto p = std::make_unique<FunctionParameterSelfAst>(
        ast_clone(conv),
        ast_clone(var));
    p->type = ast_clone(type);
    return p;
}


spp::asts::FunctionParameterSelfAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(conv);
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterSelfAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(var);
    SPP_PRINT_END;
}


auto spp::asts::FunctionParameterSelfAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default analysis steps.
    FunctionParameterAst::stage_7_analyse_semantics(sm, meta);

    // Special mutability rules for the "self" parameter.
    const auto sym = sm->current_scope->get_var_symbol(var->extract_name());
    sym->is_mutable = ast_cast<LocalVariableSingleIdentifierAst>(var.get())->tok_mut != nullptr
        or (conv and *conv == ConventionTag::MUT);

    // Apply the convention from the attribute.
    sym->type = type->with_convention(ast_clone(conv));
}
