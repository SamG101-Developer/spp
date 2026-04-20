module spp.analyse.scopes;
import spp.analyse.errors;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts;
import spp.asts.utils;
import spp.codegen.llvm_ctx;
import spp.utils.error_formatter;
import genex;


spp::analyse::scopes::ScopeManager::ScopeManager(
    std::shared_ptr<Scope> const &global_scope,
    Scope *current_scope) :
    global_scope(global_scope),
    current_scope(current_scope ? current_scope : global_scope.get()) {
}


auto spp::analyse::scopes::ScopeManager::iter() const
    -> ScopeRange {
    return ScopeRange{current_scope};
}


auto spp::analyse::scopes::ScopeManager::reset(
    Scope *scope,
    std::optional<ScopeIterator> iterator)
    -> void {
    // Set the current scope to the provided scope or global scope.
    current_scope = scope ? scope : global_scope.get();
    m_it = iterator.has_value() ? *iterator : ScopeIterator{current_scope};
}


auto spp::analyse::scopes::ScopeManager::create_and_move_into_new_scope(
    ScopeName const &name,
    AbstractAst *ast,
    spp::utils::errors::ErrorFormatter *error_formatter)
    -> Scope* {
    // Create a new scope, using the current scope as the parent scope.
    auto scope = std::make_unique<Scope>(name, current_scope, ast, error_formatter);
    current_scope->children.emplace_back(std::move(scope));
    ++m_it;

    // Set the new scope as the current scope, and advance the iterator to match.
    current_scope = current_scope->children.back().get();
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::move_out_of_current_scope()
    -> Scope* {
    // Exit the current scope into the parent scope.
    current_scope = current_scope->parent;
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::move_to_next_scope(
    const bool ignore_alias_class_scopes)
    -> Scope* {
    // For debugging mode only, check if the iterator has reached the end of the generator.
    // Move to the next scope by advancing the iterator.
    current_scope = *++m_it;
    while (ignore_alias_class_scopes and current_scope->ty_sym != nullptr and utils::scope_utils::associated_type_symbol(*current_scope)->alias_stmt != nullptr) {
        current_scope = *++m_it;
    }
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::exhaust_scope()
    -> void {
    // Manual scope skipping.
    const auto final_scope = current_scope->final_child_scope();
    while (current_scope != final_scope) {
        move_to_next_scope(false);
    }
}


auto spp::analyse::scopes::ScopeManager::cleanup()
    -> void {
    utils::scope_utils::normal_sup_blocks.clear();
    utils::scope_utils::generic_sup_blocks.clear();
}
