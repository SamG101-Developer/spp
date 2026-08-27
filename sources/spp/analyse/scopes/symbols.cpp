module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbols;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_sym_info;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
spp::analyse::scopes::Symbol::~Symbol() = default;

spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
  Shared<asts::IdentifierAst> name,
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

auto spp::analyse::scopes::NamespaceSymbol::NeedsDeepCopy() const
  -> bool {
  // Nothing about a namespace is per-instantiation.
  return false;
}

auto spp::analyse::scopes::NamespaceSymbol::operator==(
  NamespaceSymbol const &that) const
  -> bool {
  // Equality done by pointer.
  return this == &that;
}

spp::analyse::scopes::VariableSymbol::VariableSymbol(
  Shared<asts::IdentifierAst> name,
  Shared<asts::TypeAst> type,
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
  LlvmInfo = MakeShared<codegen::LlvmVarSymInfo>();
  CompTimeValue = nullptr;
}

spp::analyse::scopes::VariableSymbol::VariableSymbol(
  VariableSymbol const &that) :
  Name(AstCloneShared(that.Name)),
  Type(that.Type),
  ScopeDefinedIn(that.ScopeDefinedIn),
  IsMutable(that.IsMutable),
  IsGeneric(that.IsGeneric),
  IsFlowNarrowing(that.IsFlowNarrowing),
  Visibility(that.Visibility),
  VisibilityAnnotation(that.VisibilityAnnotation),
  MemInfo(that.MemInfo->Clone()),
  LlvmInfo(MakeShared<codegen::LlvmVarSymInfo>()),
  CompTimeValue(asts::AstClone(that.CompTimeValue)),
  AliasSym(that.AliasSym) {
  LlvmInfo->Alloca = that.LlvmInfo->Alloca;
}

spp::analyse::scopes::VariableSymbol::~VariableSymbol() = default;

auto spp::analyse::scopes::VariableSymbol::NeedsDeepCopy() const
  -> bool {
  // The memory state and the alloca belong to one instantiation.
  return true;
}

auto spp::analyse::scopes::VariableSymbol::operator==(
  VariableSymbol const &that) const
  -> bool {
  return this == &that;
}

auto spp::analyse::scopes::TypeSymbol::IsCopyable() const
  -> bool {
  return IsDirectlyCopyable
    or (DerivesFromSym != nullptr and DerivesFromSym->IsCopyable());
}

auto spp::analyse::scopes::TypeSymbol::IsZeroType() const
  -> bool {
  return IsDirectlyZeroType or (DerivesFromSym != nullptr and DerivesFromSym->IsZeroType());
}

auto spp::analyse::scopes::VariableSymbol::BoundCompValue() const
  -> asts::ExpressionAst* {
  // Only a comp generic carries a binding, and only once an argument has been given for it.
  if (not IsGeneric or MemInfo == nullptr or MemInfo->AstCompTime == nullptr) { return nullptr; }

  // An instantiation records the argument the parameter was bound to; a template records the parameter itself, which
  // is a declaration rather than a value, so it is not a binding.
  const auto bound = MemInfo->AstCompTime->To<asts::GenericArgumentCompAst>();
  return bound != nullptr ? bound->Val.get() : nullptr;
}

auto spp::analyse::scopes::VariableSymbol::FqName() const
  -> Shared<asts::ExpressionAst> {
  if (IsGeneric) { return Name; }

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
  for (auto qualifier_scope : scopes | genex::views::reverse | genex::views::drop(1)) {
    const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.get();
    auto ns_name = MakeShared<asts::IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
    auto ns_op = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, std::move(ns_name));
    qualified_name = MakeUnique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));
    qualifier_scope = qualifier_scope->Parent;
  }

  auto ns_op = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, asts::AstCloneShared(Name));
  qualified_name = MakeUnique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));

  // Return the qualified expression (either IdentifierAst or PostfixExpressionAst)
  return qualified_name;
}

spp::analyse::scopes::TypeSymbol::TypeSymbol(
  Shared<asts::TypeIdentifierAst> name,
  asts::ClassPrototypeAst *type,
  Scope *scope,
  Scope *scope_defined_in,
  Scope *scope_module,
  const bool is_generic,
  const bool is_directly_copyable,
  const asts::utils::Visibility visibility,
  Unique<asts::ConventionAst> &&convention,
  Vec<Shared<asts::TypeAst>> const &generic_constraints) :
  Name(std::move(name)),
  Type(type),
  LinkedScope(scope),
  ScopeDefinedIn(scope_defined_in),
  ScopeModule(scope_module),
  IsGeneric(is_generic),
  GenericConstraints(generic_constraints),
  IsDirectlyCopyable(is_directly_copyable),
  Visibility(visibility),
  Convention(std::move(convention)),
  GenericImpl(this),
  LlvmInfo(MakeShared<codegen::LlvmTypeSymInfo>()),
  IsDirectlyZeroType(false) {
}

spp::analyse::scopes::TypeSymbol::TypeSymbol(TypeSymbol const &that) :
  Name(that.Name),
  Type(that.Type),
  LinkedScope(that.LinkedScope),
  ScopeDefinedIn(that.ScopeDefinedIn),
  ScopeModule(that.ScopeModule),
  IsGeneric(that.IsGeneric),
  IsVariadic(that.IsVariadic),
  GenericConstraints(that.GenericConstraints),
  GenericVal(that.GenericVal),
  IsDirectlyCopyable(that.IsDirectlyCopyable),
  DerivesFromSym(that.DerivesFromSym),
  Visibility(that.Visibility),
  Convention(asts::AstClone(that.Convention)),
  GenericImpl(that.GenericImpl),
  IsDirectlyZeroType(that.IsDirectlyZeroType) {
  // Shared rather than cloned: an alias's description is fixed once resolved, and an instantiation of a generic
  // alias builds its own (see "CreateGenericClsScope") rather than mutating one it was handed.
  Alias = that.Alias;
  LlvmInfo = that.LlvmInfo;
}

spp::analyse::scopes::TypeSymbol::~TypeSymbol() = default;

auto spp::analyse::scopes::TypeSymbol::NeedsDeepCopy() const
  -> bool {
  // Only an alias is rewritten per instantiation.
  return Alias != nullptr;
}

auto spp::analyse::scopes::TypeSymbol::operator==(
  TypeSymbol const &that) const
  -> bool {
  return this == &that;
}

auto spp::analyse::scopes::TypeSymbol::AsClassSymbol() const
  -> TypeSymbol* {
  // Already a class, or a name with nothing behind it either way. The symbol answered with is owned by the table or by
  // the scope it links to, both of which outlive any caller, so it is borrowed rather than owned.
  const auto self = const_cast<TypeSymbol*>(this);
  if (Type != nullptr or LinkedScope == nullptr or LinkedScope->TySym == nullptr) { return self; }
  return LinkedScope->TySym.get();
}

auto spp::analyse::scopes::TypeSymbol::FqName(
  const bool ignore_dollar) const
  -> Shared<asts::TypeAst> {
  // An alias is transparent, so it answers with the type it resolves to rather than with its own name.
  if (Alias != nullptr) {
    return Alias->Resolved;
  }

  // If the type is generic, or is "Self", return the name as-is.
  if (IsGeneric or LinkedScope == nullptr or Name->IsSelfType()) {
    return Name;
  }

  if (Name->IsCompilerGeneratedType()
    and (ignore_dollar or LinkedScope->Parent != LinkedScope->ParentModule())) {
    return Name;
  }

  // Everything above returns a name that already exists. What is left builds one, walking the scopes above the linked
  // scope and minting an ast node per namespace part, so it is worth not doing twice: the walk reads only the shape of
  // the scope tree, which is fixed until a scope is re-parented.
  if (_CachedFqNameGen == ScopeLinkageGeneration()) {
    return _CachedFqName;
  }

  // Fully qualify the name from the root scope.
  auto qualifier_scope = LinkedScope->Parent;
  auto qualified_name = dynamic_shared_cast<asts::TypeAst>(Name);
  while (qualifier_scope->Parent != nullptr) {
    while (std::holds_alternative<ScopeBlockName>(qualifier_scope->Name)) {
      qualifier_scope = qualifier_scope->Parent;
    }
    const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.get();
    auto ns_name = MakeShared<asts::IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
    auto ns_op = MakeShared<asts::TypeUnaryExpressionOperatorNamespaceAst>(std::move(ns_name), nullptr);
    qualified_name = MakeShared<asts::TypeUnaryExpressionAst>(std::move(ns_op), std::move(qualified_name));
    qualifier_scope = qualifier_scope->Parent;
  }

  // Re-add the convention of the type if it exists.
  _CachedFqName = Convention ? qualified_name->WithConvention(asts::AstClone(Convention)) : qualified_name;
  _CachedFqNameGen = ScopeLinkageGeneration();
  return _CachedFqName;
}

auto spp::analyse::scopes::TypeSymbol::InvalidateFqNameCache() const
  -> void {
  _CachedFqName = nullptr;
  _CachedFqNameGen = 0;
}

auto spp::analyse::scopes::TypeSymbol::BoundName() const
  -> Shared<asts::TypeAst> {
  // Not a parameter, so there is no binding to follow and
  // the name is the whole answer.
  if (not IsGeneric) { return FqName(); }

  // Bound to a real type: that type's own name, carrying
  // over whatever convention the binding was written with.
  if (LinkedScope != nullptr and LinkedScope->TySym != nullptr and LinkedScope->TySym.get() != this) {
    auto bound = LinkedScope->TySym->FqName();
    return Convention != nullptr ? bound->WithConvention(asts::AstClone(Convention)) : bound;
  }

  // Bound to another parameter, which has no scope of its own
  // to reach: the recorded argument is the only record of the
  // binding. Failing that, unbound, and it stands for itself.
  return GenericVal != nullptr ? GenericVal : Name;
}

SPP_MOD_END
