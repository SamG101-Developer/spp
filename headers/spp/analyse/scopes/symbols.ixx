module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbols;
import spp.abstract;
import spp.asts;
import spp.asts.utils;
import spp.analyse.scopes;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_ctx;
import spp.analyse.utils.mem_info_utils;
import std;


namespace spp::analyse::scopes {
    SPP_EXP_CLS struct Symbol;
    SPP_EXP_CLS struct NamespaceSymbol;
    SPP_EXP_CLS struct TypeSymbol;
    SPP_EXP_CLS struct VariableSymbol;
}


/**
 * The base Symbol type for all symb variations to inherit from. This provides a common interface for all symbols, and
 * some abstract methods that must be implemented by all derived classes. The `@c Symbol* type is used, creating the
 * need for a base class.
 */
SPP_EXP_CLS struct spp::analyse::scopes::Symbol : AbstractSymbol {
    /**
     * Enforce a virtual destructor for the Symbol class. This is to ensure that derived classes can be properly
     * destructed when deleted through a base class pointer. This is important for polymorphism and memory management,
     * as it allows for proper cleanup of resources when a derived class is deleted.
     */
    virtual ~Symbol();
};


SPP_EXP_CLS struct spp::analyse::scopes::NamespaceSymbol final : Symbol {
    std::shared_ptr<asts::IdentifierAst> name;

    Scope *scope;

    NamespaceSymbol(
        std::shared_ptr<asts::IdentifierAst> name,
        Scope *scope);

    NamespaceSymbol(
        NamespaceSymbol const &that);

    ~NamespaceSymbol() override;

    auto operator==(
        NamespaceSymbol const &that) const
        -> bool;
};


SPP_EXP_CLS struct spp::analyse::scopes::VariableSymbol final : Symbol {
    std::shared_ptr<asts::IdentifierAst> name;

    std::shared_ptr<asts::TypeAst> type;

    Scope *scope_defined_in;

    bool is_mutable = false;

    bool is_generic = false;

    asts::utils::Visibility visibility;

    std::unique_ptr<utils::mem_info_utils::MemoryInfo> memory_info;

    std::unique_ptr<codegen::LlvmVarSymInfo> llvm_info;

    std::unique_ptr<void> comptime_value;

    std::shared_ptr<VariableSymbol> alias_sym;

    VariableSymbol(
        std::shared_ptr<asts::IdentifierAst> name,
        std::shared_ptr<asts::TypeAst> type,
        Scope *scope_defined_in,
        bool is_mutable = false,
        bool is_generic = false,
        asts::utils::Visibility visibility = asts::utils::Visibility::PUBLIC);

    VariableSymbol(
        VariableSymbol const &that);

    ~VariableSymbol() override;

    auto operator==(
        VariableSymbol const &that) const
        -> bool;

    SPP_ATTR_NODISCARD auto fq_name() const
        -> std::shared_ptr<asts::ExpressionAst>;
};


SPP_EXP_CLS struct spp::analyse::scopes::TypeSymbol final : Symbol {
    std::shared_ptr<asts::TypeIdentifierAst> name;

    asts::ClassPrototypeAst *type;

    Scope *scope;

    Scope *scope_defined_in;

    Scope *scope_module;

    bool is_generic = false;

    bool is_directly_copyable = false;

    std::function<bool()> is_copyable;

    asts::utils::Visibility visibility;

    std::unique_ptr<asts::ConventionAst> convention;

    TypeSymbol *generic_impl;

    std::shared_ptr<codegen::LlvmTypeSymInfo> llvm_info;

    std::unique_ptr<asts::TypeStatementAst> alias_stmt;

    std::vector<std::shared_ptr<TypeSymbol>> aliased_by_symbols = {};

    TypeSymbol(
        std::shared_ptr<asts::TypeIdentifierAst> name,
        asts::ClassPrototypeAst *type,
        Scope *scope,
        Scope *scope_defined_in,
        Scope *scope_module = nullptr,
        bool is_generic = false,
        bool is_directly_copyable = false,
        asts::utils::Visibility visibility = asts::utils::Visibility::PUBLIC,
        std::unique_ptr<asts::ConventionAst> &&convention = nullptr);

    TypeSymbol(
        TypeSymbol const &that);

    ~TypeSymbol() override;

    auto operator==(
        TypeSymbol const &that) const
        -> bool;

    SPP_ATTR_NODISCARD auto fq_name(bool ignore_dollar = true) const
        -> std::shared_ptr<asts::TypeAst>;
};
