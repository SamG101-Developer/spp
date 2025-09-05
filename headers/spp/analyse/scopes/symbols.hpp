#pragma once

#include <spp/asts/_fwd.hpp>
#include <spp/asts/utils/visbility.hpp>

namespace spp::analyse::scopes {
    class Scope;
    struct Symbol;
    struct NamespaceSymbol;
    struct TypeSymbol;
    struct AliasSymbol;
    struct VariableSymbol;
}


namespace spp::analyse::utils::mem_utils {
    struct MemoryInfo;
}


/**
 * The base Symbol type for all symb variations to inherit from. This provides a common interface for all symbols, and
 * some abstract methods that must be implemented by all derived classes. The `@c Symbol* type is used, creating the
 * need for a base class.
 */
struct spp::analyse::scopes::Symbol {
    /**
     * Enforce a virtual destructor for the Symbol class. This is to ensure that derived classes can be properly
     * destructed when deleted through a base class pointer. This is important for polymorphism and memory management,
     * as it allows for proper cleanup of resources when a derived class is deleted.
     */
    virtual ~Symbol() = default;

    /**
     * Enforce a string conversion operator for the Symbol class. This is to ensure that all derived classes can be
     * converted to a string representation. This is useful for debugging and logging purposes, as it allows for easy
     * JSON serialization of symbols.
     * @return A string representation of the symbol, which can be used for debugging or logging purposes.
     */
    virtual explicit operator std::string() const = 0;
};


struct spp::analyse::scopes::NamespaceSymbol final : Symbol {
    std::shared_ptr<asts::IdentifierAst> name;

    Scope *scope;

    NamespaceSymbol(std::shared_ptr<asts::IdentifierAst> name, Scope *scope);

    explicit operator std::string() const override;

    auto operator==(NamespaceSymbol const &that) const -> bool;
};


struct spp::analyse::scopes::VariableSymbol final : Symbol {
    std::shared_ptr<asts::IdentifierAst> name;

    std::shared_ptr<asts::TypeAst> type;

    bool is_mutable = false;

    bool is_generic = false;

    asts::utils::Visibility visibility;

    std::unique_ptr<utils::mem_utils::MemoryInfo> memory_info;

    VariableSymbol(
        std::shared_ptr<asts::IdentifierAst> name,
        std::shared_ptr<asts::TypeAst> type,
        bool is_mutable = false,
        bool is_generic = false,
        asts::utils::Visibility visibility = asts::utils::Visibility::PUBLIC);

    explicit operator std::string() const override;

    auto operator==(VariableSymbol const &that) const -> bool;
};


struct spp::analyse::scopes::TypeSymbol : Symbol {
    std::shared_ptr<asts::TypeIdentifierAst> name;

    asts::ClassPrototypeAst const *type;

    Scope *scope;

    Scope *scope_defined_in;

    bool is_generic = false;

    bool is_copyable = false;

    asts::utils::Visibility visibility = asts::utils::Visibility::PUBLIC;

    asts::ConventionAst *convention = nullptr;

    TypeSymbol *generic_impl;

    TypeSymbol(
        std::shared_ptr<asts::TypeIdentifierAst> name,
        asts::ClassPrototypeAst const *type,
        Scope *scope,
        Scope *scope_defined_in,
        bool is_generic = false,
        bool is_copyable = false,
        asts::utils::Visibility visibility = asts::utils::Visibility::PUBLIC,
        asts::ConventionAst *convention = nullptr);

    explicit operator std::string() const override;

    auto operator==(TypeSymbol const &that) const -> bool;

    virtual auto fq_name() const -> std::shared_ptr<asts::TypeAst>;
};


struct spp::analyse::scopes::AliasSymbol final : TypeSymbol {
    TypeSymbol *old_sym = nullptr;

    AliasSymbol(
            std::shared_ptr<asts::TypeIdentifierAst> name,
            asts::ClassPrototypeAst const *type,
            Scope *scope,
            Scope *scope_defined_in,
            TypeSymbol *old_sym,
            bool is_generic = false,
            bool is_copyable = false,
            asts::utils::Visibility visibility = asts::utils::Visibility::PUBLIC,
            asts::ConventionAst *convention = nullptr
        );

    explicit operator std::string() const override;

    auto operator==(AliasSymbol const &that) const -> bool;

    auto fq_name() const -> std::shared_ptr<asts::TypeAst> override;
};
