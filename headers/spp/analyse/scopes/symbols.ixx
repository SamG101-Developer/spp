module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.utils.visibility;
import spp.codegen.llvm_sym_info;
import std;


namespace spp::asts {
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

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
SPP_EXP_CLS struct spp::analyse::scopes::Symbol {
    /**
     * Enforce a virtual destructor for the Symbol class. This is to ensure that derived classes can be properly
     * destructed when deleted through a base class pointer. This is important for polymorphism and memory management,
     * as it allows for proper cleanup of resources when a derived class is deleted.
     */
    virtual ~Symbol();

    /**
     * Enforce a string conversion operator for the Symbol class. This is to ensure that all derived classes can be
     * converted to a string representation. This is useful for debugging and logging purposes, as it allows for easy
     * JSON serialization of symbols.
     * @return A string representation of the symbol, which can be used for debugging or logging purposes.
     */
    virtual explicit operator std::string() const = 0;
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

    explicit operator std::string() const override;

    auto operator==(
        NamespaceSymbol const &that) const
        -> bool;
};


SPP_EXP_CLS struct spp::analyse::scopes::VariableSymbol final : Symbol {
    std::shared_ptr<asts::IdentifierAst> name;

    std::shared_ptr<asts::TypeAst> type;

    bool is_mutable = false;

    bool is_generic = false;

    asts::utils::Visibility visibility;

    std::unique_ptr<utils::mem_info_utils::MemoryInfo> memory_info;

    std::unique_ptr<codegen::LlvmVarSymInfo> llvm_info;

    VariableSymbol(
        std::shared_ptr<asts::IdentifierAst> name,
        std::shared_ptr<asts::TypeAst> type,
        bool is_mutable = false,
        bool is_generic = false,
        asts::utils::Visibility visibility = asts::utils::Visibility::PUBLIC);

    VariableSymbol(
        VariableSymbol const &that);

    ~VariableSymbol() override;

    explicit operator std::string() const override;

    auto operator==(
        VariableSymbol const &that) const
        -> bool;
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

    std::unique_ptr<codegen::LlvmTypeSymInfo> llvm_info;

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

    explicit operator std::string() const override;

    auto alias_stmt() const
        -> asts::TypeStatementAst*;

    auto operator==(
        TypeSymbol const &that) const
        -> bool;

    SPP_ATTR_NODISCARD auto fq_name() const
        -> std::shared_ptr<asts::TypeAst>;
};


spp::analyse::scopes::VariableSymbol::~VariableSymbol() = default;

spp::analyse::scopes::TypeSymbol::~TypeSymbol() = default;
