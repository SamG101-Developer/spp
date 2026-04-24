module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbols;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_sym_info;
import genex;


SPP_MOD_BEGIN
spp::analyse::scopes::Symbol::~Symbol() = default;


spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
    std::shared_ptr<asts::IdentifierAst> name,
    Scope *scope) :
    name(std::move(name)),
    scope(scope) {
}


spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
    NamespaceSymbol const &that) :
    name(that.name),
    scope(that.scope) {
}


spp::analyse::scopes::NamespaceSymbol::~NamespaceSymbol() = default;


auto spp::analyse::scopes::NamespaceSymbol::operator==(
    NamespaceSymbol const &that) const
    -> bool {
    return this == &that;
}


spp::analyse::scopes::VariableSymbol::VariableSymbol(
    std::shared_ptr<asts::IdentifierAst> name,
    std::shared_ptr<asts::TypeAst> type,
    Scope *scope_defined_in,
    const bool is_mutable,
    const bool is_generic,
    const asts::utils::Visibility visibility) :
    name(std::move(name)),
    type(std::move(type)),
    scope_defined_in(scope_defined_in),
    is_mutable(is_mutable),
    is_generic(is_generic),
    visibility(visibility),
    memory_info(std::make_unique<utils::mem_info_utils::MemoryInfo>()) {
    llvm_info = std::make_unique<codegen::LlvmVarSymInfo>();
    comptime_value = nullptr;
}


spp::analyse::scopes::VariableSymbol::VariableSymbol(
    VariableSymbol const &that) :
    name(ast_clone(that.name)),
    type(ast_clone(that.type)),
    scope_defined_in(that.scope_defined_in),
    is_mutable(that.is_mutable),
    is_generic(that.is_generic),
    visibility(that.visibility),
    memory_info(that.memory_info->clone()),
    llvm_info(std::make_unique<codegen::LlvmVarSymInfo>()) {
    llvm_info->alloca = that.llvm_info->alloca;
}


spp::analyse::scopes::VariableSymbol::~VariableSymbol() = default;


auto spp::analyse::scopes::VariableSymbol::operator==(
    VariableSymbol const &that) const
    -> bool {
    return this == &that;
}


auto spp::analyse::scopes::VariableSymbol::fq_name() const
    -> std::shared_ptr<asts::ExpressionAst> {
    if (is_generic) { return name; }

    // Fully qualify the name from the root scope.
    auto qualifier_scope = scope_defined_in;
    auto scopes = std::vector<Scope*>();

    while (qualifier_scope->parent != nullptr) {
        while (std::holds_alternative<ScopeBlockName>(qualifier_scope->name)) {
            qualifier_scope = qualifier_scope->parent;
        }
        scopes.emplace_back(qualifier_scope);
        qualifier_scope = qualifier_scope->parent;
    }

    auto qualified_name = std::unique_ptr<asts::ExpressionAst>(nullptr);
    qualified_name = asts::ast_clone(std::get<ScopeIdentifierName>(scopes.back()->name).name);
    for (auto qualifier_scope : scopes | genex::views::reverse | genex::views::drop(1)) {
        const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->name).name.get();
        auto ns_name = std::make_shared<asts::IdentifierAst>(raw_ns_name->pos_start(), raw_ns_name->val);
        auto ns_op = std::make_unique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, std::move(ns_name));
        qualified_name = std::make_unique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));
        qualifier_scope = qualifier_scope->parent;
    }

    auto ns_op = std::make_unique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, asts::ast_clone(name));
    qualified_name = std::make_unique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));

    // Return the qualified expression (either IdentifierAst or PostfixExpressionAst)
    return qualified_name;
}


spp::analyse::scopes::TypeSymbol::TypeSymbol(
    std::shared_ptr<asts::TypeIdentifierAst> name,
    asts::ClassPrototypeAst *type,
    Scope *scope,
    Scope *scope_defined_in,
    Scope *scope_module,
    const bool is_generic,
    const bool is_directly_copyable,
    const asts::utils::Visibility visibility,
    std::unique_ptr<asts::ConventionAst> &&convention) :
    name(std::move(name)),
    type(type),
    scope(scope),
    scope_defined_in(scope_defined_in),
    scope_module(scope_module),
    is_generic(is_generic),
    is_directly_copyable(is_directly_copyable),
    is_copyable([this] { return this->is_directly_copyable; }),
    visibility(visibility),
    convention(std::move(convention)),
    generic_impl(this),
    llvm_info(std::make_unique<codegen::LlvmTypeSymInfo>()) {
}


spp::analyse::scopes::TypeSymbol::TypeSymbol(TypeSymbol const &that) :
    name(that.name),
    type(that.type),
    scope(that.scope),
    scope_defined_in(that.scope_defined_in),
    scope_module(that.scope_module),
    is_generic(that.is_generic),
    is_directly_copyable(that.is_directly_copyable),
    is_copyable(that.is_copyable),
    visibility(that.visibility),
    convention(asts::ast_clone(that.convention)),
    generic_impl(that.generic_impl) {
    alias_stmt = asts::ast_clone(that.alias_stmt);
    llvm_info = that.llvm_info;
}


spp::analyse::scopes::TypeSymbol::~TypeSymbol() = default;


auto spp::analyse::scopes::TypeSymbol::operator==(
    TypeSymbol const &that) const
    -> bool {
    return this == &that;
}


auto spp::analyse::scopes::TypeSymbol::fq_name(
    const bool ignore_dollar) const
    -> std::shared_ptr<asts::TypeAst> {
    // For aliases, return the fully qualified name of the aliased type.
    if (alias_stmt != nullptr) {
        return alias_stmt->m_mapped_old_type;
    }

    // If the type is generic, or the name starts with a '$', return the name as-is.
    if (is_generic or scope == nullptr or (ignore_dollar and name->name[0] == '$')) {
        return name;
    }

    // Fully qualify the name from the root scope.
    auto qualifier_scope = scope->parent;
    auto qualified_name = std::dynamic_pointer_cast<asts::TypeAst>(name);
    while (qualifier_scope->parent != nullptr) {
        while (std::holds_alternative<ScopeBlockName>(qualifier_scope->name)) {
            qualifier_scope = qualifier_scope->parent;
        }
        const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->name).name.get();
        auto ns_name = std::make_shared<asts::IdentifierAst>(raw_ns_name->pos_start(), raw_ns_name->val);
        auto ns_op = std::make_unique<asts::TypeUnaryExpressionOperatorNamespaceAst>(std::move(ns_name), nullptr);
        qualified_name = std::make_shared<asts::TypeUnaryExpressionAst>(std::move(ns_op), std::move(qualified_name));
        qualifier_scope = qualifier_scope->parent;
    }

    // Re-add the convention of the type if it exists.
    return convention ? qualified_name->with_convention(ast_clone(convention)) : qualified_name;
}

SPP_MOD_END
