module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbols;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.generic_argument_group_ast;
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
import spp.utils.ptr;

SPP_MOD_BEGIN
spp::analyse::scopes::Symbol::~Symbol() = default;

spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
    asts::IdentifierAst *name,
    Scope *scope) :
    Name(std::move(name)),
    LinkedScope(scope) {
}

spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
    NamespaceSymbol const &that) :
    Name(that.Name),
    LinkedScope(that.LinkedScope) {
}

spp::analyse::scopes::NamespaceSymbol::~NamespaceSymbol() = default;

auto spp::analyse::scopes::NamespaceSymbol::operator==(
    NamespaceSymbol const &that) const
    -> bool {
    // Equality done by pointer.
    return this == &that;
}

spp::analyse::scopes::VariableSymbol::VariableSymbol(
    asts::IdentifierAst *name,
    Unique<asts::TypeAst> &&type,
    Scope *ScopeDefinedIn,
    const bool is_mutable,
    const bool is_generic,
    const asts::utils::Visibility visibility) :
    Name(std::move(name)),
    Type(std::move(type)),
    ScopeDefinedIn(ScopeDefinedIn),
    IsMutable(is_mutable),
    IsGeneric(is_generic),
    Visibility(visibility),
    MemInfo(MakeUnique<utils::mem_info_utils::MemoryInfo>()) {
    // Todo: Initializer list?
    LlvmInfo = MakeUnique<codegen::LlvmVarSymInfo>();
    CompTimeValue = nullptr;
    AliasSym = nullptr;
}

spp::analyse::scopes::VariableSymbol::VariableSymbol(
    VariableSymbol const &that) :
    Name(that.Name),
    Type(asts::AstClone(that.Type)),
    ScopeDefinedIn(that.ScopeDefinedIn),
    IsMutable(that.IsMutable),
    IsGeneric(that.IsGeneric),
    Visibility(that.Visibility),
    VisibilityAnnotation(that.VisibilityAnnotation),
    MemInfo(that.MemInfo->Clone()),
    LlvmInfo(MakeUnique<codegen::LlvmVarSymInfo>()) {
    LlvmInfo->Alloca = that.LlvmInfo->Alloca;
    AliasSym = that.AliasSym; // ?
}

spp::analyse::scopes::VariableSymbol::~VariableSymbol() = default;

auto spp::analyse::scopes::VariableSymbol::operator==(
    VariableSymbol const &that) const
    -> bool {
    return this == &that;
}

auto spp::analyse::scopes::VariableSymbol::FqName() const
    -> Unique<asts::ExpressionAst> {
    if (IsGeneric) { return AstClone(Name); }

    // Fully qualify the name from the root scope.
    auto qualifier_scope = ScopeDefinedIn;
    auto scopes = Vec<Scope*>();

    while (qualifier_scope->Parent != nullptr) {
        while (std::holds_alternative<ScopeBlockName>(qualifier_scope->Name)) {
            qualifier_scope = qualifier_scope->Parent;
        }
        scopes.EmplaceBack(qualifier_scope);
        qualifier_scope = qualifier_scope->Parent;
    }

    auto qualified_name = Unique<asts::ExpressionAst>(nullptr);
    qualified_name = asts::AstClone(std::get<ScopeIdentifierName>(scopes.Back()->Name).Name);
    for (auto qualifier_scope : scopes | std::views::reverse | std::views::drop(1)) {
        const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.Get();
        auto ns_name = MakeUnique<asts::IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
        auto ns_op = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, std::move(ns_name));
        qualified_name = MakeUnique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));
        qualifier_scope = qualifier_scope->Parent;
    }

    auto ns_op = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, asts::AstClone(Name));
    qualified_name = MakeUnique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));

    // Return the qualified expression (either IdentifierAst or PostfixExpressionAst)
    return qualified_name;
}

spp::analyse::scopes::TypeSymbol::TypeSymbol(
    asts::TypeIdentifierAst *name,
    asts::ClassPrototypeAst *type,
    Scope *scope,
    Scope *scope_defined_in,
    Scope *scope_module,
    const bool is_generic,
    const bool is_directly_copyable,
    const asts::utils::Visibility visibility,
    Unique<asts::ConventionAst> &&convention,
    Vec<asts::TypeAst*> const &generic_constraints) :
    Name(std::move(name)),
    Type(type),
    LinkedScope(scope),
    ScopeDefinedIn(scope_defined_in),
    ScopeModule(scope_module),
    IsGeneric(is_generic),
    GenericConstraints(generic_constraints),
    IsDirectlyCopyable(is_directly_copyable),
    IsCopyable([this] { return this->IsDirectlyCopyable; }),
    Visibility(visibility),
    Convention(std::move(convention)),
    GenericImpl(this),
    LlvmInfo(MakeUnique<codegen::LlvmTypeSymInfo>()),
    IsDirectlyZeroType(false),
    IsZeroType([this] { return this->IsDirectlyZeroType; }) {
    if (this == reinterpret_cast<TypeSymbol*>(0x9e2f7a0)) {
        auto _ = 123;
    }
}

spp::analyse::scopes::TypeSymbol::TypeSymbol(TypeSymbol const &that) :
    Name(that.Name),
    Type(that.Type),
    LinkedScope(that.LinkedScope),
    ScopeDefinedIn(that.ScopeDefinedIn),
    ScopeModule(that.ScopeModule),
    IsGeneric(that.IsGeneric),
    GenericConstraints(that.GenericConstraints),
    IsDirectlyCopyable(that.IsDirectlyCopyable),
    IsCopyable(that.IsCopyable),
    Visibility(that.Visibility),
    Convention(asts::AstClone(that.Convention)),
    GenericImpl(that.GenericImpl),
    IsDirectlyZeroType(that.IsDirectlyZeroType),
    IsZeroType(that.IsZeroType) {
    AliasStmt = asts::AstClone(that.AliasStmt);
    LlvmInfo = that.LlvmInfo->Clone();
}

spp::analyse::scopes::TypeSymbol::~TypeSymbol() = default;

auto spp::analyse::scopes::TypeSymbol::operator==(
    TypeSymbol const &that) const
    -> bool {
    return this == &that;
}

auto spp::analyse::scopes::TypeSymbol::FqName(
    const bool ignore_dollar) const
    -> asts::TypeAst* {
    if (_Fq != nullptr) { return _Fq.Get(); }

    // For aliases, return the fully qualified name of the aliased type.
    if (AliasStmt != nullptr) {
        _Fq = asts::AstClone(AliasStmt->MappedOldType);
        return _Fq.Get();
    }

    // If the type is generic, or the name starts with a '$', return the name as-is.
    if (IsGeneric or LinkedScope == nullptr or (ignore_dollar and Name->IsCompilerGeneratedType()) or Name->Name == "Self") {
        _Fq = asts::AstClone(Name);
        return _Fq.Get();
    }

    // Fully qualify the name from the root scope.
    auto qualifier_scope = LinkedScope->Parent;
    auto qualified_name = AstClone(Name->To<asts::TypeAst>());
    while (qualifier_scope->Parent != nullptr) {
        while (std::holds_alternative<ScopeBlockName>(qualifier_scope->Name)) {
            qualifier_scope = qualifier_scope->Parent;
        }
        const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.Get();
        auto ns_name = MakeUnique<asts::IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
        auto ns_op = MakeUnique<asts::TypeUnaryExpressionOperatorNamespaceAst>(std::move(ns_name), nullptr);
        qualified_name = MakeUnique<asts::TypeUnaryExpressionAst>(std::move(ns_op), std::move(qualified_name));
        qualifier_scope = qualifier_scope->Parent;
    }

    // Re-add the convention of the type if it exists.
    _Fq = Convention != nullptr ? qualified_name->WithConvention(asts::AstClone(Convention)) : std::move(qualified_name);
    return _Fq.Get();
}

SPP_MOD_END
