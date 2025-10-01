#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/iter_pattern_variant_variable_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/object_initializer_ast.hpp>
#include <spp/asts/object_initializer_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>


spp::asts::IterPatternVariantVariableAst::IterPatternVariantVariableAst(
    decltype(var) &&var) :
    IterPatternVariantAst(),
    var(std::move(var)) {
}


spp::asts::IterPatternVariantVariableAst::~IterPatternVariantVariableAst() = default;


auto spp::asts::IterPatternVariantVariableAst::pos_start() const
    -> std::size_t {
    return var->pos_start();
}


auto spp::asts::IterPatternVariantVariableAst::pos_end() const
    -> std::size_t {
    return var->pos_end();
}


auto spp::asts::IterPatternVariantVariableAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterPatternVariantVariableAst>(
        ast_clone(var));
}


spp::asts::IterPatternVariantVariableAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantVariableAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_END;
}


auto spp::asts::IterPatternVariantVariableAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create a dummy type with the same type as the variable's type, to initialize it.
    auto dummy_type = meta->case_condition->infer_type(sm, meta)->type_parts().back()->generic_arg_group->type_at("Yield")->val;
    const auto conv = dummy_type->get_convention();
    auto dummy = std::make_unique<ObjectInitializerAst>(std::move(dummy_type), nullptr);

    // Create a new AST node that initializes the variable with the dummy value.
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, std::move(dummy));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);

    // Update borrow flags if the "Yield" type has a borrow convention attached to it.
    for (auto &&name : m_mapped_let->var->extract_names()) {
        // Apply the borrow to the symbol.
        const auto sym = sm->current_scope->get_var_symbol(name);
        sym->memory_info->ast_borrowed = conv;
        sym->memory_info->is_borrow_mut = conv and *conv == ConventionAst::ConventionTag::MUT;
        sym->memory_info->is_borrow_ref = conv and *conv == ConventionAst::ConventionTag::REF;
        sym->type = sym->type->with_convention(ast_clone(conv));
    }
}


auto spp::asts::IterPatternVariantVariableAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory state of the variable.
    m_mapped_let->stage_8_check_memory(sm, meta);
}
