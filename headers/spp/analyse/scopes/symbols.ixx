module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.utils.visibility;
import spp.codegen.llvm_sym_info;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
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
    SPP_GCC_VTABLE_FIX_BASE

    /**
     * Enforce a virtual destructor for the Symbol class. This is to ensure that derived classes can be properly
     * destructed when deleted through a base class pointer. This is important for polymorphism and memory management,
     * as it allows for proper cleanup of resources when a derived class is deleted.
     */
    virtual ~Symbol();
};

SPP_EXP_CLS struct spp::analyse::scopes::NamespaceSymbol final : Symbol {
    SPP_GCC_VTABLE_FIX

    asts::IdentifierAst *Name;

    Scope *LinkedScope;

    NamespaceSymbol(
        asts::IdentifierAst *name,
        Scope *scope);

    NamespaceSymbol(
        NamespaceSymbol const &that);

    ~NamespaceSymbol() override;

    auto operator==(
        NamespaceSymbol const &that) const
        -> bool;
};

SPP_EXP_CLS struct spp::analyse::scopes::VariableSymbol final : Symbol {
    SPP_GCC_VTABLE_FIX

    asts::IdentifierAst *Name;

    Unique<asts::TypeAst> Type;

    Scope *ScopeDefinedIn;

    bool IsMutable = false;

    bool IsGeneric = false;

    asts::utils::Visibility Visibility;

    asts::AnnotationAst *VisibilityAnnotation = nullptr;

    Unique<utils::mem_info_utils::MemoryInfo> MemInfo;

    Unique<codegen::LlvmVarSymInfo> LlvmInfo;

    Unique<asts::Ast> CompTimeValue;

    VariableSymbol *AliasSym;

    VariableSymbol(
        asts::IdentifierAst *name,
        Unique<asts::TypeAst> &&type,
        Scope *ScopeDefinedIn,
        bool is_mutable = false,
        bool is_generic = false,
        asts::utils::Visibility visibility = asts::utils::Visibility::kPublic);

    VariableSymbol(
        VariableSymbol const &that);

    ~VariableSymbol() override;

    auto operator==(
        VariableSymbol const &that) const
        -> bool;

    SPP_ATTR_NODISCARD auto FqName() const
        -> Unique<asts::ExpressionAst>;
};

SPP_EXP_CLS struct spp::analyse::scopes::TypeSymbol final : Symbol {
    SPP_GCC_VTABLE_FIX

    asts::TypeIdentifierAst *Name;

    asts::ClassPrototypeAst *Type;

    Scope *LinkedScope;

    Scope *ScopeDefinedIn;

    Scope *ScopeModule;

    bool IsGeneric = false;

    Vec<asts::TypeAst*> GenericConstraints;

    bool IsDirectlyCopyable = false;

    Function<bool()> IsCopyable;

    asts::utils::Visibility Visibility;

    Unique<asts::ConventionAst> Convention;

    TypeSymbol *GenericImpl;

    Unique<codegen::LlvmTypeSymInfo> LlvmInfo;

    Unique<asts::TypeStatementAst> AliasStmt;

    Vec<TypeSymbol*> AliasedBySyms;

    bool IsDirectlyZeroType;

    Function<bool()> IsZeroType;

    TypeSymbol(
        asts::TypeIdentifierAst *name,
        asts::ClassPrototypeAst *type,
        Scope *scope,
        Scope *scope_defined_in,
        Scope *scope_module = nullptr,
        bool is_generic = false,
        bool is_directly_copyable = false,
        asts::utils::Visibility visibility = asts::utils::Visibility::kPublic,
        Unique<asts::ConventionAst> &&convention = nullptr,
        Vec<asts::TypeAst*> const &generic_constraints = {});

    TypeSymbol(
        TypeSymbol const &that);

    ~TypeSymbol() override;

    auto operator==(
        TypeSymbol const &that) const
        -> bool;

    SPP_ATTR_NODISCARD auto FqName(bool ignore_dollar = true) const -> asts::TypeAst*;

    auto ResetFqCache() -> void;

private:
    mutable Unique<asts::TypeAst> _Fq;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::Symbol)
SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::NamespaceSymbol)
SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::VariableSymbol)
SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::TypeSymbol)
