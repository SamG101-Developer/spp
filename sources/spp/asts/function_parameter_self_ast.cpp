module;
#include <spp/macros.hpp>

module spp.asts;
import spp.analyse.scopes;
import spp.analyse.utils.scope_utils;
import spp.asts.utils;
import :common_types;


spp::asts::FunctionParameterSelfAst::FunctionParameterSelfAst(
    decltype(conv) &&conv,
    decltype(var) &&var) :
    FunctionParameterAst(std::move(var), nullptr, nullptr, utils::OrderableTag::SELF_PARAM),
    conv(std::move(conv)) {
    type = common_types::self_type(pos_start());
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


auto spp::asts::FunctionParameterSelfAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default analysis steps.
    FunctionParameterAst::stage_7_analyse_semantics(sm, meta);

    // Special mutability rules for the "self" parameter.
    const auto sym = analyse::utils::scope_utils::get_var_symbol(sm->current_scope, var->extract_name());
    sym->is_mutable = var->to<LocalVariableSingleIdentifierAst>()->tok_mut != nullptr
        or (conv and *conv == ConventionTag::MUT);

    // Apply the convention from the attribute.
    sym->type = type->with_convention(ast_clone(conv));
}
