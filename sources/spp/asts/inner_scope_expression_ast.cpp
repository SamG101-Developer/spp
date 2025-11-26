#include <spp/analyse/errors/semantic_error.ixx>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/loop_control_flow_statement_ast.hpp>
#include <spp/asts/ret_statement_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>

#include <genex/views/enumerate.hpp>


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = ast_cast<InnerScopeAst<T>>(InnerScopeAst<T>::clone().release());
    return std::make_unique<InnerScopeExpressionAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::new_empty()
    -> std::unique_ptr<InnerScopeExpressionAst> {
    return std::make_unique<InnerScopeExpressionAst>(
        nullptr, decltype(InnerScopeExpressionAst::members)(), nullptr);
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create a scope for the InnerScopeAst node.
    auto scope_name = analyse::scopes::ScopeBlockName("<inner-scope#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    m_scope = sm->current_scope;

    // Check for unreachable code.
    for (auto &&[i, member] : this->members | genex::views::ptr | genex::views::enumerate) {
        auto ret_stmt = ast_cast<RetStatementAst>(member);
        auto loop_flow_stmt = ast_cast<LoopControlFlowStatementAst>(member);
        if ((ret_stmt or loop_flow_stmt) and (member != this->members.back().get())) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppUnreachableCodeError>().with_args(
                *member, *this->members[i + 1]).with_scopes({sm->current_scope}).raise();
        }
    }

    // Analyse the members of the inner scope.
    for (auto const &x : this->members) { x->stage_7_analyse_semantics(sm, meta); }
    sm->move_out_of_current_scope();
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // If there are any members, return the last member's inferred type.
    if (not this->members.empty()) {
        auto tm = ScopeManager(sm->global_scope, m_scope);
        return this->members.back()->infer_type(&tm, meta);
    }

    // Otherwise, return the void type.
    return generate::common_types::void_type(pos_start());
}


template struct spp::asts::InnerScopeExpressionAst<std::unique_ptr<spp::asts::StatementAst>>;
