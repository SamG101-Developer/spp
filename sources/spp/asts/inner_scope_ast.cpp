#include <algorithm>

#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/loop_control_flow_statement_ast.hpp>
#include <spp/asts/ret_statement_ast.hpp>
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>
#include <spp/asts/token_ast.hpp>

#include <genex/actions/remove.hpp>
#include <genex/views/enumerate.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/view.hpp>


template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst(
    decltype(tok_l) &&tok_l,
    decltype(members) &&members,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    members(std::move(members)),
    tok_r(std::move(tok_r)) {
}


// template <typename T>
// spp::asts::InnerScopeAst<T>::~InnerScopeAst() = default;


template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst() :
    tok_l(nullptr),
    members(),
    tok_r(nullptr) {
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<InnerScopeAst<T>>(
        ast_clone(tok_l),
        ast_clone_vec(members),
        ast_clone(tok_r));
}


template <typename T>
spp::asts::InnerScopeAst<T>::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(members);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(members);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create a scope for the InnerScopeAst node.
    auto scope_name = analyse::scopes::ScopeBlockName("<inner-scope#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    m_scope = sm->current_scope;

    // Analyse the members of the inner scope.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
    sm->move_out_of_current_scope();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move into the next scope.
    sm->move_to_next_scope();

    // Check the memory of each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
    auto all_syms = sm->current_scope->all_var_symbols();
    auto inner_syms = sm->current_scope->all_var_symbols(true);

    // Invalidate yielded borrows that are linked.
    for (auto &&sym : inner_syms) {
        for (auto &&pin : sym->memory_info->ast_pins | genex::views::view | genex::views::to<std::vector>()) {
            const auto pin_sym = sm->current_scope->get_var_symbol(ast_cast<IdentifierAst>(*pin));
            for (auto &&info : pin_sym->memory_info->borrow_refers_to | genex::views::view | genex::views::to<std::vector>()) {
                pin_sym->memory_info->borrow_refers_to |= genex::actions::remove_if([sym](auto &&x) { return *ast_cast<IdentifierAst>(std::get<0>(x)) == *sym->name; });
                pin_sym->memory_info->borrow_refers_to |= genex::actions::remove_if([info](auto &&x) { return std::get<0>(x) == std::get<0>(info); });
            }
        }
    }

    for (auto &&sym : all_syms) {
        for (auto &&bor : sym->memory_info->borrow_refers_to | genex::views::view | genex::views::to<std::vector>()) {
            auto [a, b, _, scope] = bor;
            if (scope == sm->current_scope) {
                sym->memory_info->borrow_refers_to |= genex::actions::remove(bor);
            }
        }
    }

    // If the final expression of the inner scope is being used (ie assigned ot outer variable), then memory check it.
    if (const auto move = meta->assignment_target; not members.empty() and move != nullptr) {
        if (auto expr_member = ast_cast<ExpressionAst>(final_member())) {
            analyse::utils::mem_utils::validate_symbol_memory(*expr_member, *move, sm, true, true, true, true, true, true, meta);
        }
    }

    sm->move_out_of_current_scope();
}


template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::ClassMemberAst>>;
template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::FunctionMemberAst>>;
template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::SupMemberAst>>;
