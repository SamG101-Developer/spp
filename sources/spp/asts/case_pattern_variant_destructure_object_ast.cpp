#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/case_pattern_variant_destructure_object_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/let_statement_uninitialized_ast.hpp>
#include <spp/asts/local_variable_destructure_object_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/local_variable_single_identifier_alias_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/pch.hpp>


spp::asts::CasePatternVariantDestructureObjectAst::CasePatternVariantDestructureObjectAst(
    decltype(type) &&type,
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    type(std::move(type)),
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
}


spp::asts::CasePatternVariantDestructureObjectAst::~CasePatternVariantDestructureObjectAst() = default;


auto spp::asts::CasePatternVariantDestructureObjectAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::CasePatternVariantDestructureObjectAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::CasePatternVariantDestructureObjectAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantDestructureObjectAst>(
        ast_clone(type),
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::CasePatternVariantDestructureObjectAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureObjectAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantDestructureObjectAst::convert_to_variable(
    mixins::CompilerMetaData *meta)
    -> std::unique_ptr<LocalVariableAst> {
    // Recursively map the elements to their local variable counterparts.
    auto mapped_elems = elems
        | genex::views::transform([meta](auto const &x) { return x->convert_to_variable(meta); })
        | genex::to<std::vector>();

    // Create the final local variable wrapping, tag it and return it.
    auto var = std::make_unique<LocalVariableDestructureObjectAst>(ast_clone(type), nullptr, std::move(mapped_elems), nullptr);
    var->m_from_pattern = true;
    return var;
}


auto spp::asts::CasePatternVariantDestructureObjectAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Analyse the class type (required for flow typing).
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(type)->fq_name();

    // Get the condition symbol if it exists.
    auto cond_sym = sm->current_scope->get_var_symbol(ast_clone(ast_cast<IdentifierAst>(meta->case_condition)));
    auto cond = meta->case_condition;
    if (cond_sym == nullptr) {
        auto cond_type = meta->case_condition->infer_type(sm, meta);

        // Create a variable and let statement for the condition.
        auto var_name = std::make_shared<IdentifierAst>(pos_start(), std::format("$_{}", reinterpret_cast<std::uintptr_t>(this)));
        auto var_ast = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, var_name, nullptr);
        const auto let_ast = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var_ast), cond_type, nullptr, ast_clone(meta->case_condition));
        let_ast->stage_7_analyse_semantics(sm, meta);

        // Set the memory information of the symbol based on the type of iteration.
        cond = var_name.get();
        cond_sym = sm->current_scope->get_var_symbol(var_name);
    }

    // Flow type the condition symbol if necessary.
    const auto is_cond_type_variant = analyse::utils::type_utils::is_type_variant(*cond_sym->type, *sm->current_scope);
    if (is_cond_type_variant) {
        if (not analyse::utils::type_utils::symbolic_eq(*cond_sym->type, *type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *meta->case_condition, *cond_sym->type, *type, *type).with_scopes({sm->current_scope}).raise();
        }

        const auto flow_sym = std::make_shared<analyse::scopes::VariableSymbol>(*cond_sym);
        flow_sym->type = type;
        sm->current_scope->add_var_symbol(flow_sym);
    }

    // Create the new variable from the pattern in the patterns scope.
    auto var = convert_to_variable(meta);
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, ast_clone(cond));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantDestructureObjectAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Forward memory checking to the mapped let statement.
    m_mapped_let->stage_8_check_memory(sm, meta);
}
