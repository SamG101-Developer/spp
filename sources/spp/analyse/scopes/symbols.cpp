#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_mov_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_unary_expression_ast.hpp>
#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>
#include <spp/asts/token_ast.hpp>

#include <nlohmann/json.hpp>


spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
    std::shared_ptr<asts::IdentifierAst> name,
    Scope *scope) :
    name(std::move(name)),
    scope(scope) {
}


spp::analyse::scopes::NamespaceSymbol::operator std::string() const {
    return nlohmann::json(std::map<std::string, std::string>{
        {"what", "type"},
        {"name", static_cast<std::string>(*name)},
        {"scope", static_cast<std::string>(*std::get<asts::IdentifierAst*>(scope->name))}
    }).dump();
}


auto spp::analyse::scopes::NamespaceSymbol::operator==(
    NamespaceSymbol const &that) const
    -> bool {
    return this == &that;
}


spp::analyse::scopes::VariableSymbol::VariableSymbol(
    std::shared_ptr<asts::IdentifierAst> name,
    std::shared_ptr<asts::TypeAst> type,
    const bool is_mutable,
    const bool is_generic,
    const asts::utils::Visibility visibility) :
    name(std::move(name)),
    type(std::move(type)),
    is_mutable(is_mutable),
    is_generic(is_generic),
    visibility(visibility),
    memory_info(std::make_unique<utils::mem_utils::MemoryInfo>()) {
}


spp::analyse::scopes::VariableSymbol::operator std::string() const {
    return nlohmann::json(std::map<std::string, std::string>{
        {"what", "variable"},
        {"name", static_cast<std::string>(*name)},
        {"type", static_cast<std::string>(*type)}
    }).dump();
}


auto spp::analyse::scopes::VariableSymbol::operator==(
    VariableSymbol const &that) const
    -> bool {
    return this == &that;
}


spp::analyse::scopes::TypeSymbol::TypeSymbol(
    std::shared_ptr<asts::TypeIdentifierAst> name,
    asts::ClassPrototypeAst const *type,
    Scope *scope,
    Scope *scope_defined_in,
    const bool is_generic,
    const bool is_copyable,
    const asts::utils::Visibility visibility,
    asts::ConventionAst *convention) :
    name(std::move(name)),
    type(type),
    scope(scope),
    scope_defined_in(scope_defined_in),
    is_generic(is_generic),
    is_copyable(is_copyable),
    visibility(visibility),
    convention(convention),
    generic_impl(nullptr) {
}


spp::analyse::scopes::TypeSymbol::operator std::string() const {
    return nlohmann::json(std::map<std::string, std::string>{
        {"what", "type"},
        {"name", static_cast<std::string>(*name)},
        {"defined_in", static_cast<std::string>(*std::get<asts::IdentifierAst*>(scope_defined_in->name))},
        {"scope", static_cast<std::string>(*std::get<asts::IdentifierAst*>(scope->name))}
    }).dump();
}


auto spp::analyse::scopes::TypeSymbol::operator==(
    TypeSymbol const &that) const
    -> bool {
    return this == &that;
}


auto spp::analyse::scopes::TypeSymbol::fq_name() const
    -> std::shared_ptr<asts::TypeAst> {
    // If the type is generic, or the name starts with a '$', return the name as-is.
    if (is_generic or name->name[0] == '$') {
        return name;
    }

    // Map "Self" types to the fully qualified name of the type via the scope.
    if (name->name == "Self") {
        return scope->ty_sym->fq_name();
    }

    // Fully qualify the name from the root scope.
    auto qualifier_scope = scope->parent;
    auto qualified_name = std::dynamic_pointer_cast<asts::TypeAst>(name);
    while (qualifier_scope->parent != nullptr) {
        const auto raw_ns_name = std::get<asts::IdentifierAst*>(qualifier_scope->name);
        auto ns_name = std::make_shared<asts::IdentifierAst>(raw_ns_name->pos_start(), raw_ns_name->val);
        auto ns_op = std::make_unique<asts::TypeUnaryExpressionOperatorNamespaceAst>(std::move(ns_name), nullptr);
        qualified_name = std::make_shared<asts::TypeUnaryExpressionAst>(std::move(ns_op), std::move(qualified_name));
        qualifier_scope = qualifier_scope->parent;
    }

    // Re-add the convention of the type if it exists.
    auto conv = convention ? ast_clone(convention) : std::make_unique<asts::ConventionMovAst>();
    return qualified_name->with_convention(std::move(conv));
}


spp::analyse::scopes::AliasSymbol::AliasSymbol(
    std::shared_ptr<asts::TypeIdentifierAst> name,
    asts::ClassPrototypeAst const *type,
    Scope *scope,
    Scope *scope_defined_in,
    TypeSymbol *old_sym,
    const bool is_generic,
    const bool is_copyable,
    const asts::utils::Visibility visibility,
    asts::ConventionAst *convention) :
    TypeSymbol(std::move(name), type, scope, scope_defined_in, is_generic, is_copyable, visibility, convention),
    old_sym(old_sym) {
}


spp::analyse::scopes::AliasSymbol::operator std::string() const {
    return nlohmann::json(std::map<std::string, std::string>{
        {"what", "alias"},
        {"name", static_cast<std::string>(*name)},
        {"defined_in", static_cast<std::string>(*std::get<asts::IdentifierAst*>(scope_defined_in->name))},
        {"scope", static_cast<std::string>(*std::get<asts::IdentifierAst*>(scope->name))},
        {"old_sym", old_sym != nullptr ? static_cast<std::string>(*old_sym->name) : "<null>"}
    }).dump();
}


auto spp::analyse::scopes::AliasSymbol::operator==(
    AliasSymbol const &that) const
    -> bool {
    return this == &that;
}


auto spp::analyse::scopes::AliasSymbol::fq_name(
    ) const
    -> std::shared_ptr<asts::TypeAst> {
    // Alias symbols' qualified names are just the symbol's name (die to alias mapping).
    return name;
}
