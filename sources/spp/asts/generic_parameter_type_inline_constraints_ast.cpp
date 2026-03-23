module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::GenericParameterTypeInlineConstraintsAst::GenericParameterTypeInlineConstraintsAst(
    decltype(tok_colon) &&tok_colon,
    std::vector<std::unique_ptr<TypeAst>> && constraints) :
    tok_colon(std::move(tok_colon)) {
    for (auto &&constraint : constraints) {
        this->constraints.emplace_back(std::move(constraint));
    }
}


spp::asts::GenericParameterTypeInlineConstraintsAst::~GenericParameterTypeInlineConstraintsAst() = default;


auto spp::asts::GenericParameterTypeInlineConstraintsAst::new_empty()
    -> std::unique_ptr<GenericParameterTypeInlineConstraintsAst> {
    return std::make_unique<GenericParameterTypeInlineConstraintsAst>(
        nullptr, std::vector<std::unique_ptr<TypeAst>>{});
}


auto spp::asts::GenericParameterTypeInlineConstraintsAst::pos_start() const
    -> std::size_t {
    return tok_colon->pos_start();
}


auto spp::asts::GenericParameterTypeInlineConstraintsAst::pos_end() const
    -> std::size_t {
    return constraints.empty() ? tok_colon->pos_end() : constraints.back()->pos_end();
}


auto spp::asts::GenericParameterTypeInlineConstraintsAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterTypeInlineConstraintsAst>(
        ast_clone(tok_colon),
        ast_clone_vec(constraints));
}


spp::asts::GenericParameterTypeInlineConstraintsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_EXTEND(constraints, " & ");
    SPP_STRING_END;
}


auto spp::asts::GenericParameterTypeInlineConstraintsAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Prepare the fully qualified constraints vector.
    auto fq_constraints = decltype(constraints)();
    fq_constraints.reserve(constraints.size());

    // Analyse each constraint type.
    for (auto const &constraint : constraints) {
        constraint->stage_7_analyse_semantics(sm,  meta);
        auto const constraint_type_sym = sm->current_scope->get_type_symbol(constraint->without_generics());
        fq_constraints.emplace_back(constraint_type_sym->fq_name()->with_generics(std::move(constraint->type_parts().back()->generic_arg_group)));
    }

    // Replace the constraints with their fully qualified versions.
    constraints = std::move(fq_constraints);
}
