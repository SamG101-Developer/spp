#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/generate/common_types.hpp>


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> {
    // If there are any members, return the last member's inferred type.
    if (not this->members->empty()) {
        auto tm = ScopeManager(sm->global_scope, m_scope);
        return this->members->back()->infer_type(&tm, meta);
    }

    // Otherwise, return the void type.
    return generate::common_types::void_type(pos_start());
}
