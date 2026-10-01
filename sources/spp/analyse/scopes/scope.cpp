module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbol_table;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.comptime_intrinsics;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.module_prototype_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.compiler.module_tree;
import spp.utils.algorithms;
import spp.utils.error_formatter;
import spp.utils.interner;
import spp.utils.ptr;
import genex;

namespace spp::analyse::scopes {
  namespace {
    /// The fully qualified name that a super scope contributes
    /// as a super type, handling generics too, using a class
    /// symbol vs scope's TySym.
    auto ResolveSupTypeName(Scope const *scope) -> Shared<TypeAst> {
      // The scope's own type symbol names it: an instantiation
      // with its arguments, a class or template as written.
      if (scope->TySym != nullptr) { return scope->TySym->FqName(); }
      const auto cls_proto = AstAs<ClassPrototypeAst>(scope->AstNode);
      const auto cls_sym = cls_proto != nullptr ? cls_proto->GetClsSym() : nullptr;
      return cls_sym != nullptr ? cls_sym->FqName() : nullptr;
    }

    /// Search a scope's direct super-scopes for a variable
    /// symbol with the given name. Recursively moves through all
    /// super scopes in the tree, to retrieve a compile-time
    /// constant. Exclusive searches are used to remain in the sup
    /// graph and not escaping upwards into enclosing lexical
    /// scopes.
    auto SearchSupScopesForVar(Scope const &scope, IdentifierAst const *name) -> VariableSymbol* {
      if (Scope::OnSupScopesRead) { Scope::OnSupScopesRead(scope); }
      for (const auto sup_scope : scope.DirectSupScopes) {
        const auto sym = sup_scope->GetVarSymbol(name, true);
        if (sym != nullptr) { return sym; }
      }
      return nullptr;
    }

    /// Search a scope's direct super-scopes for a type symbol
    /// with the given name. Recursively moves through all
    /// super scopes in the tree, to retrieve a sup-bound type
    /// Exclusive searches are used to remain in the sup graph
    /// and not escaping upwards into enclosing lexical scopes.
    auto SearchSupScopesForType(Scope const &scope, TypeIdentifierAst const *name) -> TypeSymbol* {
      if (Scope::OnSupScopesRead) { Scope::OnSupScopesRead(scope); }
      for (const auto sup_scope : scope.DirectSupScopes) {
        const auto sym = sup_scope->GetTypeSymbol(name, true);
        if (sym != nullptr) { return sym; }
      }
      return nullptr;
    }

    /// The symbol "scope"'s super scopes file for a parameter identity, searched as a type name is ("sup_search"):
    /// each direct super scope, then its own.
    template <typename Sym, typename ByParam>
    auto SearchSupScopesByParam(Scope const &scope, const std::uint64_t id, ByParam const &by_param) -> Sym* {
      for (const auto sup_scope : scope.SupScopes()) {
        if (auto *const found = by_param(*sup_scope, id); found != nullptr) { return found; }
      }
      return nullptr;
    }

    /// The binding a scope gives a generic parameter, by the parameter's identity ("IndividualSymbolTable::GetByParam"):
    /// the nearest scope filing one, where a scope asks its own table, then (inside a "sup" block, whose class's
    /// parameters are bound by the block's "Self" instantiation) that instantiation's; then the super scopes. A scope
    /// that files only the parameter itself (or a copy of it) leaves it unbound there, and the search goes on for a
    /// binding further out. Nothing binding it leaves the parameter itself.
    template <typename Sym, typename ByParam>
    auto FindParamBindingById(Scope const &from, Sym &param, const std::uint64_t id, ByParam const &by_param) -> Sym* {
      static const auto self_name = TypeIdentifierAst::FromString("Self");
      const auto binding = [&](Sym *found) -> Sym* {
        if (found == nullptr or found->BindsParamId != id) { return nullptr; }
        if constexpr (std::same_as<std::remove_const_t<Sym>, TypeSymbol>) { found->FollowBoundAlias(); }
        return found;
      };
      for (auto scope = &from; scope != nullptr; scope = scope->Parent) {
        if (auto *const found = binding(by_param(*scope, id)); found != nullptr) { return found; }
        if (const auto self_sym = scope->InternalTable.TypeTbl.Get(self_name.get());
          self_sym != nullptr and self_sym->IsSelf() and self_sym->LinkedScope != nullptr
          and self_sym->LinkedScope != scope) {
          if (auto *const found = binding(by_param(*self_sym->LinkedScope, id)); found != nullptr) { return found; }
        }
      }
      if (auto *const found = binding(SearchSupScopesByParam<Sym>(from, id, by_param)); found != nullptr) { return found; }
      return &param;
    }

    /// The 2 memoisation generations, starting at "1" because
    /// "0" means "never computed" rather than "computed before
    /// anything moved".
    std::uint64_t _ScopeLinkageGeneration = 1;
    std::uint64_t _TypeStructureGeneration = 1;
  }
}

SPP_MOD_BEGIN
Scope::Scope(ScopeName name, Scope *parent, Ast *ast, ErrorFormatter *error_formatter) :
  Name(std::move(name)),
  Parent(parent),
  AstNode(ast),
  TySym(nullptr),
  NsSym(nullptr),
  NonGenericScope(this),
  _ErrorFormatter(error_formatter) {
}

Scope::Scope(Scope const &other) :
  Name(other.Name),
  Parent(other.Parent),
  AstNode(other.AstNode),
  TySym(other.TySym),
  NsSym(other.NsSym),
  NonGenericScope(other.NonGenericScope),
  Deferred(other.Deferred),
  _ErrorFormatter(nullptr) {
  InternalTable.ShallowCopyFrom(other.InternalTable);

  // Copy the children recursively.
  for (auto const &child_scope : other.Children) {
    auto child_copy = MakeUnique<Scope>(*child_scope);
    child_copy->Parent = this;
    Children.EmplaceBack(std::move(child_copy));
  }
}

Scope::~Scope() = default;

auto Scope::NewGlobal(Module const &mod)
  -> Shared<Scope> {
  // Create a new global scope (no parent or ast for the global
  // scope). This is a master scope for all scope managers.
  auto scope_name = ScopeBlockName::FromParts(
    "__global__", {}, 0);
  auto glob_scope = MakeShared<Scope>(
    std::move(scope_name), nullptr, nullptr, mod.error_formatter.get());

  // Inject the "_global" namespace symbol into this scope to make
  // lookups orthogonal.
  auto glob_ns_sym_name = MakeShared<IdentifierAst>(0uz, "_global");
  auto glob_ns_sym = MakeShared<NamespaceSymbol>(
    std::move(glob_ns_sym_name), glob_scope.get());
  glob_scope->NsSym = std::move(glob_ns_sym);

  // Return the global scope.
  return glob_scope;
}

auto Scope::ShiftForNamespacedType(
  Scope const &scope, TypeAst const &fq_type)
  -> Pair<const Scope*, TypeIdentifierAst const*> {
  // Note: the sole caller (GetTypeSymbol) only reaches here
  // for non-TypeIdentifier types, so there is always at least
  // one namespace or nested-type part to shift through.

  // Get the namespace and type parts, to get the scopes. Use
  // the appending form, so a type of any depth only uses one
  // allocation per list rather than one per level of the chain.
  auto ns_parts = Vec<IdentifierAst const*>();
  auto type_parts = Vec<TypeIdentifierAst const*>();
  fq_type.NsPartsInto(ns_parts);
  fq_type.TypePartsInto(type_parts);
  auto shifted_scope = &scope;

  // Iterate to move through the namespace parts first.
  for (auto const *ns_part : ns_parts) {
    const auto sym = shifted_scope->GetNsSymbol(ns_part);
    if (sym == nullptr) { break; }
    shifted_scope = sym->LinkedScope;
  }

  // Iterate through the type parts (except the final one) next.
  for (auto const *type_part : type_parts | genex::views::drop_last(1)) {
    const auto sym = shifted_scope->GetTypeSymbol(type_part);
    if (sym == nullptr or sym->IsTypeGeneric()) { break; }
    shifted_scope = sym->LinkedScope;
  }

  // Return the type scope, and the final type part.
  auto const *final = type_parts.Back();
  return {shifted_scope, final};
}

auto Scope::GetErrorFormatter() const -> ErrorFormatter* {
  // Return this scope's error formatter, or the parent's if
  // it doesn't exist.
  const auto this_formatter = _ErrorFormatter;
  return this_formatter != nullptr
    ? this_formatter
    : Parent->GetErrorFormatter();
}

auto Scope::IsFromPrelude(Ast const &ast) const -> bool {
  // The prelude is appended behind the author's code, so a
  // position past what they wrote is one of its nodes.
  const auto formatter = GetErrorFormatter();
  return formatter != nullptr and formatter->IsPastUserSource(ast.PosStart());
}

auto Scope::GetGenerics() const
  -> Vec<Unique<GenericArgumentAst>> {
  // Create the symbols list.
  const auto scopes = Ancestors();
  auto syms = Vec<Unique<GenericArgumentAst>>();
  auto type_names = Vec<Shared<TypeIdentifierAst>>();
  auto comp_names = Vec<Shared<IdentifierAst>>();

  // Check each ancestor scope, accumulating generic type
  // and comp symbols.
  for (const auto scope : scopes) {
    auto all_type_syms = scope->AllTypeSymbols(true)
      | genex::views::filter([](auto const &sym) { return sym->IsTypeGeneric(); })
      | genex::to<Vec>();

    auto all_var_syms = scope->AllVarSymbols(true)
      | genex::views::filter([](auto const &sym) { return sym->IsCompGeneric(); })
      | genex::to<Vec>();

    // Bindings only, of both kinds: an unbound parameter is no generic argument of this scope.
    for (auto const &t : all_type_syms) {
      if (t->Kind == TypeKind::GenericParam) { continue; }
      if (t->LinkedScope == nullptr and t->GenericVal == nullptr) { continue; }
      if (genex::contains(type_names, *t->Name, genex::meta::deref)) { continue; }
      syms.EmplaceBack(GenericArgumentAst::FromSym(*t));
      type_names.EmplaceBack(t->Name);
    }

    for (auto const &v : all_var_syms) {
      if (genex::contains(comp_names, *v->Name, genex::meta::deref)) { continue; }
      if (v->Kind == VariableKind::GenericCompParam) { continue; }
      syms.EmplaceBack(GenericArgumentAst::FromSym(*v));
      comp_names.EmplaceBack(v->Name);
    }
  }

  // Return the list of generic symbols.
  return syms;
}

auto Scope::AddVarSymbol(
  Shared<VariableSymbol> const &sym)
  -> void {
  // Add a variable symbol to the corresponding symbol table.
  InternalTable.VarTbl.Add(sym->Name.get(), sym);
}

auto Scope::AddVarSymbolCheckConflict(
  Shared<VariableSymbol> const &sym)
  -> void {
  // Cannot allow for duplicate comptime definitions.
  const auto existing_sym = GetVarSymbol(sym->Name.get(), false);
  if (existing_sym != nullptr) {
    const auto is_functional = existing_sym->Kind == VariableKind::Function;

    // A name brought in by a "use" is being shadowed by a
    // declaration written here, which is not a redefinition
    // - "use std::mem::ops::drop" alongside a "fun drop" of
    // this type's own is the ordinary case. The lookup above
    // is non-exclusive, so it reaches the module-level import
    // from inside a "sup" block.
    const auto is_shadowed_import = existing_sym->IsImport() and not sym->IsImport();

    // The prelude is appended behind the author's code, so a name
    // it imports is reported through the author's side alone.
    const auto existing_from_prelude = IsFromPrelude(*existing_sym->Name);
    if (existing_from_prelude != IsFromPrelude(*sym->Name)) {
      auto const &mine = existing_from_prelude ? *sym : *existing_sym;
      RaiseIf<errors::SppIdentifierDuplicateError>(
        not is_functional and not is_shadowed_import,
        {mine.ScopeDefinedIn ? mine.ScopeDefinedIn : this},
        ERR_ARGS(*mine.Name, "comptime variable identifier"));
    }

    RaiseIf<errors::SppIdentifierDuplicateError>(
      not is_functional and not is_shadowed_import,
      {
        existing_sym->ScopeDefinedIn ? existing_sym->ScopeDefinedIn : this,
        sym->ScopeDefinedIn ? sym->ScopeDefinedIn : this
      },
      ERR_ARGS(*existing_sym->Name, *sym->Name, "comptime variable identifier"));
  }

  // Add a variable symbol to the corresponding symbol table.
  InternalTable.VarTbl.Add(sym->Name.get(), sym);
}

auto Scope::AddTypeSymbol(
  Shared<TypeSymbol> const &sym)
  -> void {
  // Add a type symbol to the corresponding symbol table.
  InternalTable.TypeTbl.Add(sym->Name.get(), sym);
}

auto Scope::AddTypeSymbolCheckConflict(
  Shared<TypeSymbol> const &sym)
  -> void {
  // Cannot allow for duplicate definitions.
  const auto existing_sym = GetTypeSymbol(sym->Name.get(), false);
  if (existing_sym != nullptr) {
    const auto is_functional = sym->IsMock();
    const auto existing_from_prelude = IsFromPrelude(*existing_sym->Name);
    if (existing_from_prelude != IsFromPrelude(*sym->Name)) {
      auto const &mine = existing_from_prelude ? *sym : *existing_sym;
      RaiseIf<errors::SppIdentifierDuplicateError>(
        not is_functional, {mine.ScopeDefinedIn}, ERR_ARGS(*mine.Name, "type identifier"));
    }

    RaiseIf<errors::SppIdentifierDuplicateError>(
      not is_functional,
      {existing_sym->ScopeDefinedIn, sym->ScopeDefinedIn},
      ERR_ARGS(*existing_sym->Name, *sym->Name, "type identifier"));
  }

  // Add a type symbol to the corresponding symbol table.
  InternalTable.TypeTbl.Add(sym->Name.get(), sym);
}

auto Scope::AddNsSymbol(
  Shared<NamespaceSymbol> const &sym) -> void {
  // Add a namespace symbol to the corresponding symbol table.
  InternalTable.NsTbl.Add(sym->Name.get(), sym);
}

auto Scope::RemVarSymbol(
  IdentifierAst const *sym_name) -> Shared<VariableSymbol> {
  // Remove a variable symbol from the corresponding symbol table.
  return InternalTable.VarTbl.Rem(sym_name);
}

auto Scope::RemTypeSymbol(
  TypeIdentifierAst const *sym_name) -> Shared<TypeSymbol> {
  // Remove a type symbol from the corresponding symbol table.
  return InternalTable.TypeTbl.Rem(sym_name);
}

auto Scope::AllVarSymbols(
  const bool exclusive,
  const bool sup_scope_search) const
  -> Vec<VariableSymbol*> {
  // Yield all symbols from the var symbol table.
  auto syms = InternalTable.VarTbl.All();

  // For non-exclusive searches where a parent is present,
  // yield from the parent scope.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->AllVarSymbols(exclusive, sup_scope_search));
  }

  // For super scope searches, yield from all direct super
  // scopes.
  if (sup_scope_search) {
    for (auto const *sup_scope : SupScopes()) {
      syms.AppendRange(sup_scope->AllVarSymbols(true, false));
    }
  }

  return syms;
}

auto Scope::AllTypeSymbols(
  const bool exclusive,
  const bool sup_scope_search) const
  -> Vec<TypeSymbol*> {
  // Yield all symbols from the type symbol table.
  auto syms = InternalTable.TypeTbl.All();

  // For non-exclusive searches where a parent is present,
  // yield from the parent scope.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->AllTypeSymbols(exclusive, sup_scope_search));
  }

  // For super scope searches, yield from all super scopes.
  // Each is asked non-recursively, so the walk has to be over
  // the transitive closure rather than the direct list, which
  // is what the variable version above does.
  if (sup_scope_search) {
    for (auto const *sup_scope : SupScopes()) {
      syms.AppendRange(sup_scope->AllTypeSymbols(true, false));
    }
  }

  return syms;
}

auto Scope::AllNsSymbols(
  const bool exclusive, bool) const -> Vec<NamespaceSymbol*> {
  // The second parameter is a super-scope search, which a
  // namespace never has one of. It is accepted so the three
  // symbol kinds share a signature, and deliberately ignored.
  auto syms = InternalTable.NsTbl.All();

  // For non-exclusive searches where a parent is present,
  // yield from the parent scope.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->AllNsSymbols(exclusive));
  }
  return syms;
}

auto Scope::HasVarSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not. The
  // lookup is borrowed, so this costs no refcount traffic.
  return GetVarSymbol(sym_name, exclusive) != nullptr;
}

auto Scope::HasNsSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not.
  return GetNsSymbol(sym_name, exclusive) != nullptr;
}

auto Scope::GetVarSymbol(
  IdentifierAst const *sym_name,
  const bool exclusive,
  const bool sup_scope_search) const
  -> VariableSymbol* {
  // Get the symbol from the symbol table if it exists.
  if (sym_name == nullptr) { return nullptr; }

  // A name recording the comp parameter it named where it was
  // written means that parameter from here too, rather than
  // whatever its spelling names in this scope.
  if (not exclusive and sym_name->WrittenParam() != 0) {
    if (auto *const param = GenericCompParamOf(sym_name->WrittenParam()); param != nullptr) {
      if (const auto canon = CanonVar(*param); canon != nullptr) { return canon; }
    }
  }

  const auto scope = this;
  auto sym = InternalTable.VarTbl.Get(sym_name);

  // If the symbol doesn't exist, and this is a non-exclusive
  // search, check the parent scope.
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->GetVarSymbol(sym_name, exclusive, sup_scope_search);
  }

  // If the symbol still hasn't been found, check the super
  // scopes for it.
  if (sym == nullptr and sup_scope_search) {
    sym = SearchSupScopesForVar(*scope, sym_name);
  }

  // Check for a linked aliased variable symbol.
  if (sym != nullptr and sym->AliasSym != nullptr) {
    sym = sym->AliasSym.get();
  }

  // Return the found symbol, or nullptr.
  return sym;
}

auto Scope::GetTypeSymbol(
  TypeAst const *sym_name, const bool exclusive,
  const bool sup_scope_search) const -> TypeSymbol* {
  // Nullptr guard allows uniform lookups with no pre-checks
  // in callers.
  if (sym_name == nullptr) { return nullptr; }

  // A type carrying the identity it resolved to where it was
  // written means that identity from here too, rather than
  // whatever its spelling happens to name in this scope.
  if (not exclusive) {
    if (const auto written = sym_name->Written(); written != nullptr) {
      if (const auto found = ResolveWritten(written); found != nullptr) {
        return found;
      }
    }
  }

  // Adjust the scope for the namespace of the type identifier
  // if there is one.
  auto scope = this;
  auto sym_name_extracted = static_cast<TypeIdentifierAst const*>(nullptr);
  if (sym_name->IsTypeIdentifier()) {
    sym_name_extracted = sym_name->ToUnchecked<TypeIdentifierAst>();
  }
  else {
    auto [scope_, sym_name_extracted_] = ShiftForNamespacedType(*this, *sym_name);
    scope = scope_;
    sym_name_extracted = sym_name_extracted_;
  }

  // A wrapper ("&T") hands the lookup to its last part, which
  // may carry a written identity of its own.
  if (not exclusive and sym_name_extracted != sym_name) {
    if (const auto written = sym_name_extracted->Written(); written != nullptr) {
      if (const auto found = ResolveWritten(written); found != nullptr) {
        return found;
      }
    }
  }

  // An instantiation is found by its template and what its
  // arguments resolve to from here, not by its spelling.
  if (not exclusive and not sym_name_extracted->GnArgGroup->Args.IsEmpty()) {
    const auto tmpl = scope->GetTypeSymbol(sym_name_extracted->WithoutGenerics().get());
    if (tmpl != nullptr and not tmpl->Instances.empty()) {
      auto const *const params = tmpl->Alias != nullptr
        ? tmpl->Alias->Params.get()
        : tmpl->Type != nullptr
        ? tmpl->Type->GnParamGroup.get()
        : nullptr;

      const auto hit = tmpl->Instances.find(
        InstanceTypeId(*tmpl, sym_name_extracted->GnArgGroup->GetAllArgs(), params));

      if (hit != tmpl->Instances.end()) {
        return hit->second;
      }
    }
  }

  // Get the symbol from the symbol table if it exists.
  auto sym = scope->InternalTable.TypeTbl.Get(sym_name_extracted);

  // If the symbol doesn't exist, and this is a non-exclusive
  // search, check the parent scope.
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->GetTypeSymbol(sym_name_extracted, exclusive, sup_scope_search);
  }

  // If the symbol still hasn't been found, check the super
  // scopes for it.
  if (sym == nullptr and sup_scope_search) {
    sym = SearchSupScopesForType(*scope, sym_name_extracted);
  }

  // An instantiation is answered by identity, never by spelling.
  // The table holds one entry per spelling, so "Mutex[T=T]"
  // written in the class and in "sup [T: Drop] Mutex[T]" are
  // one entry and two types: the second names the block's own
  // parameter, which carries its constraints. A hit that
  // instantiates this name's template under another identity is
  // not the type being asked for.
  // Todo: Clean this (and this whole function) up.
  if (sym != nullptr and sym->InstanceOf != nullptr and not sym_name_extracted->GnArgGroup->Args.IsEmpty()) {
    if (const auto tmpl = scope->GetTypeSymbol(sym_name_extracted->WithoutGenerics().get());
      tmpl == sym->InstanceOf) {
      const auto params = tmpl->Alias != nullptr
        ? tmpl->Alias->Params.get()
        : tmpl->Type != nullptr
        ? tmpl->Type->GnParamGroup.get()
        : nullptr;

      if (sym->Id != InstanceTypeId(*tmpl, sym_name_extracted->GnArgGroup->GetAllArgs(), params)) {
        sym = nullptr;
      }
    }
  }


  // Return the found symbol, or nullptr.
  return sym;
}

auto Scope::CanonVar(VariableSymbol &sym) const -> VariableSymbol* {
  // As "Canon": a comp parameter by its identifier, and a binding by the parameter it binds.
  const auto by_param = [](Scope const &scope, const std::uint64_t id) {
    return scope.InternalTable.VarTbl.GetByParam(id);
  };
  if (sym.Kind == VariableKind::GenericCompParam and sym.ParamId != 0) {
    return FindParamBindingById(*this, sym, sym.ParamId, by_param);
  }
  if (sym.Kind == VariableKind::GenericCompArg and sym.BindsParamId != 0) {
    return FindParamBindingById(*this, sym, sym.BindsParamId, by_param);
  }
  return nullptr;
}

auto Scope::Canon(TypeSymbol &sym) const -> TypeSymbol* {
  // Only a generic parameter or generic argument can be
  // canonical-ised. For the generic parameter case, find
  // the binding by the generic parameter's parameter
  // identifier.
  if (sym.Kind == TypeKind::GenericParam and sym.ParamId != 0) {
    return FindParamBindingById(*this, sym, sym.ParamId, [](Scope const &scope, const std::uint64_t id) {
      return scope.InternalTable.TypeTbl.GetByParam(id);
    });
  }

  // For the generic argument case, handle the bound parameter
  // identifier, ie the target of the generic argument.
  if (sym.Kind == TypeKind::GenericArg and sym.BindsParamId != 0) {
    return FindParamBindingById(*this, sym, sym.BindsParamId, [](Scope const &scope, const std::uint64_t id) {
      return scope.InternalTable.TypeTbl.GetByParam(id);
    });
  }

  // A closed class names the same type from anywhere; there are
  // no generics that can be canonical-ised.
  if (sym.Kind == TypeKind::Class and sym.IsConcrete and sym.Alias == nullptr) { return &sym; }

  // An open instantiation is its identity read through this scope's bindings ("ReadIn"): the instantiation filed under
  // that. One not made yet has no answer here: a lookup makes nothing ("ResolveTypeSymbol" does).
  if (sym.InstanceOf != nullptr and (sym.Kind == TypeKind::Class or sym.Alias != nullptr)) {
    return sym.Id != nullptr ? SymbolOf(ReadIn(sym.Id)) : nullptr;
  }

  // An alias declared in a generic block has a copy per instantiation of that block, its target read through the
  // instantiation's bindings ("CreateGenericSupScope"). The copy this scope reaches is the one it means; copies share
  // the declaration's name node.
  if (sym.InstanceOf == nullptr and sym.Alias != nullptr) {
    auto *const here = GetTypeSymbol(sym.Name->WithoutGenerics().get());
    return here != nullptr and here->Name == sym.Name ? here : &sym;
  }

  // A template is its own declaration from anywhere. Only a template's written identity - the stripped head of a name
  // written with arguments - brings one here.
  if (sym.InstanceOf == nullptr and sym.Kind == TypeKind::Class) { return &sym; }
  return nullptr;
}

auto Scope::ResolveTypeSymbol(
  TypeAst const *type) const
  -> TypeSymbol* {
  // What the lookup finds; else, for a name written as an open instantiation that is not made from here yet, the one
  // its arguments name here, made now - the one place an instantiation is made on reading a type.
  if (auto *const sym = GetTypeSymbol(type); sym != nullptr) { return sym; }
  auto *const open = SymbolOf(type->LastTypePart()->Written());
  if (open == nullptr or open->InstanceOf == nullptr or not OnInstantiationMissing) { return nullptr; }
  if (open->Kind != TypeKind::Class and open->Alias == nullptr) { return nullptr; }
  return OnInstantiationMissing(*open, *this);
}

auto Scope::InstanceIdentityKey(
  Vec<GenericArgumentAst*> const &args, GenericParameterGroupAst const *params) const
  -> TypeId {
  using utils::comp_generics::CompExprIdentity;
  using Tag = InstanceKey::Tag;

  // Start with an empty instance key. This will get built upon
  // as the generics are considered.
  auto key = InstanceKey();

  // A spelling is keyed by the id the interner gives it, so what
  // is pushed is a view of text that already exists: only the
  // "TypeIdentifierAst" holds its stringification, and anything
  // else has to build one.
  const auto push_spelling = [&key](const Tag tag, TypeAst const &type) {
    if (type.IsTypeIdentifier()) { key.PushText(tag, type.ToUnchecked<TypeIdentifierAst>()->ToView()); }
    else { key.PushText(tag, type.ToString()); }
  };

  // Named by keyword where it can be, by position otherwise (a
  // tuple's arguments stay positional). A comp argument is its
  // value's identity: folded where closed, its parameters by
  // identity where not, so "n + 1" under two bindings of "n"
  // is two keys and "(n + 1)" is "n + 1".
  auto comp_identity = Str();
  for (auto i = 0uz; i < args.Len(); ++i) {
    const auto arg = args[i];
    const auto param = arg->Name == nullptr and params != nullptr and i < params->Params.Len()
      ? params->Params[i].get()
      : nullptr;

    // Push the argument name into the overall key. This works for
    // keyword arguments.
    if (arg->Name != nullptr) {
      push_spelling(Tag::Name, *arg->Name);
    }
    // If we have variadics, then the push the parameter's name,
    // which will bind the tuple of variadic arguments going in.
    else if (param != nullptr and param->TokEllipsis == nullptr) {
      push_spelling(Tag::Name, *param->Name);
    }
    // Otherwise we have a genuinely positional argument, so push
    // the index.
    else {
      key.Push(Tag::Pos, i);
    }

    // Handle type vs comp arguments, either pushing the type, or
    // Todo: document how comp works here, and CompExprIdentity.
    if (arg->TypeVal != nullptr) {
      if (const auto id = TypeIdOf(*arg->TypeVal); id != nullptr) { key.PushId(id); }
      else { key.PushKey(TypeKey(*arg->TypeVal)); }
    }
    else if (arg->CompVal != nullptr) {
      comp_identity.clear();
      CompExprIdentity(*arg->CompVal, *this, comp_identity);
      key.PushText(Tag::Comp, comp_identity);
      // A closed value is recorded as what it folds to, anything else as written (its parameters named by identity).
      // A pack's elements are recorded each on its own too, as a substitution rewrites them one at a time.
      // Its names recording the parameters they name here first, so the recorded value means the same wherever it is
      // rebuilt ("CompValueOf").
      auto folded = utils::comp_generics::FoldCompExpr(*arg->CompVal, *this);
      auto const &value = folded != nullptr ? *folded : *arg->CompVal;
      utils::comp_generics::RecordCompGenerics(value, *this);
      NoteCompValue(comp_identity, value);
      if (auto const *const tup = value.To<TupleLiteralAst>(); tup != nullptr) {
        auto elem_identity = Str();
        for (auto const &elem : tup->Elems) {
          elem_identity.clear();
          CompExprIdentity(*elem, *this, elem_identity);
          NoteCompValue(elem_identity, *elem);
        }
      }
    }
  }
  return InternTypeKey(std::move(key));
}

namespace {
  /// A type key, and when the type is a variant, the keys of its members, so a variant member of a variant is
  /// flattened into it.
  struct TypeKeyParts {
    spp::analyse::scopes::InstanceKey Key;
    spp::Vec<spp::analyse::scopes::InstanceKey> Members;
  };

  /// The aliases being keyed through their targets: a target that names its own alias (directly or down a chain) is keyed
  /// as the alias inside itself rather than expanded without end.
  thread_local auto keying_aliases = std::vector<spp::analyse::scopes::TypeSymbol const*>();
}

namespace {
  /// "args" as the instantiation they name would record them: one per parameter, in parameter order, named after it -
  /// a keyword argument where given, else the next positional one, else the parameter's default. A type default is its
  /// identity, read where the parameters are declared ("decl_scope"), with what is named so far bound; a comp default
  /// has what is named so far substituted into its expression. Empty when the parameters are variadic, whose packing
  /// this does not attempt, or when a default does not resolve.
  auto RecordedArgsFor(
    spp::asts::GenericArgumentGroupAst const &args,
    spp::asts::GenericParameterGroupAst const &params,
    spp::analyse::scopes::Scope const &scope,
    spp::analyse::scopes::Scope const &decl_scope)
    -> spp::Vec<spp::Unique<spp::asts::GenericArgumentAst>> {
    using namespace spp::asts;
    auto out = spp::Vec<spp::Unique<GenericArgumentAst>>();
    if (params.GetVariadicParams() != nullptr) { return out; }
    // Positional arguments name the parameters of their own kind in order, as the solver names them ("NameOfKind").
    auto const positional = args.GetPositionalArgs();
    auto positional_of = [&positional](const bool comp) {
      return positional
        | ::genex::views::filter([comp](auto const *arg) { return (arg->CompVal != nullptr) == comp; })
        | ::genex::to<spp::Vec>();
    };
    auto const positional_types = positional_of(false);
    auto const positional_comps = positional_of(true);
    auto next_type = 0uz;
    auto next_comp = 0uz;
    for (auto const &param : params.Params) {
      const auto comp = param->CompType != nullptr;
      auto const *given = args.At(param->Name->LastTypePart()->Name.c_str());
      if (given == nullptr and not comp and next_type < positional_types.Len()) { given = positional_types[next_type++]; }
      if (given == nullptr and comp and next_comp < positional_comps.Len()) { given = positional_comps[next_comp++]; }
      const auto so_far = out | ::genex::views::ptr | ::genex::to<spp::Vec>();
      if (given != nullptr and given->TypeVal != nullptr) { out.EmplaceBack(GenericArgumentAst::NewType(param->Name, given->TypeVal)); }
      else if (given != nullptr and given->CompVal != nullptr) {
        out.EmplaceBack(GenericArgumentAst::NewComp(param->Name, AstClone(given->CompVal)));
      }
      else if (param->TypeDefault != nullptr) {
        const auto bindings = spp::analyse::utils::type_resolution::BindArgs(params, so_far, scope);
        auto type = scope.TypeAstOf(SubstituteTypeId(decl_scope.TypeIdOf(*param->TypeDefault), bindings));
        if (type == nullptr) { return {}; }
        out.EmplaceBack(GenericArgumentAst::NewType(param->Name, std::move(type)));
      }
      // The default as written, as the solver takes it: analysing it desugars its operators ("n.add(1_uz)").
      else if (auto const *const written = param->WrittenCompDefault != nullptr
          ? param->WrittenCompDefault.get()
          : param->CompDefault.get();
        written != nullptr) {
        out.EmplaceBack(GenericArgumentAst::NewComp(param->Name, AstClone(written->SubstituteGenericsExpr(so_far))));
      }
    }
    return out;
  }
}

namespace {
  /// The key of "t" read in "scope" ("Scope::TypeKey"), and when it is a variant, the keys of its members, so a
  /// variant member of a variant is flattened into it.
  auto TypeKeyIn(spp::analyse::scopes::Scope const &scope, spp::asts::TypeAst const &t) -> TypeKeyParts;

  /// The key of a written type ("t"), or of a type already resolved to "given" (a "TypeRef"'s symbol), read in "scope".
  auto KeyIn(
    spp::analyse::scopes::Scope const &scope,
    spp::asts::TypeAst const *t,
    spp::analyse::scopes::TypeSymbol const *given)
    -> TypeKeyParts {
    using namespace spp::analyse::scopes;
    using namespace spp::asts;
    using Tag = InstanceKey::Tag;

    auto out = TypeKeyParts();
    auto &key = out.Key;
    const auto push_spelling = [&key](const Tag tag, TypeAst const &x) {
      if (x.IsTypeIdentifier()) { key.PushText(tag, x.ToUnchecked<TypeIdentifierAst>()->ToView()); }
      else { key.PushText(tag, x.ToString()); }
    };

    // A convention is part of the type. "Self" is keyed by its spelling, before it is read as anything: its meaning
    // is per scope, and an argument left as "Self" is carried into an instance's own sup blocks, where it is a new
    // instance at each level - keyed by meaning, "Vec[Self]" minted "View[T=Self]", "IterRef[T=Self]", etc.
    if (t != nullptr) {
      if (const auto conv = t->GetConvention(); conv != nullptr) {
        key.Push(Tag::Conv, static_cast<std::uint64_t>(conv->Tag()));
      }
      if (t->IsSelfType()) {
        key.Push(Tag::Self);
        return out;
      }
    }

    // A key already made, continued here: a variant's members are carried with it, so a variant it is a member of
    // flattens it.
    const auto append_id = [&](spp::analyse::scopes::TypeId const id) {
      key.AppendKey(*id);
      auto const &head = HeadOf(id);
      if (head.Kind == Tag::Variant and head.Conv == 0) {
        for (const auto m : head.Members) { out.Members.EmplaceBack(InstanceKey(*m)); }
      }
    };

    // A made instantiation is the identity it was filed under ("Scope::InstanceTypeId").
    const auto instance_key = [&](TypeSymbol const &inst) { append_id(inst.Id); };

    // A name written as an open instantiation is its written identity read here ("ReadIn"); one written as an
    // instantiation of an alias is the alias's target read here, guarded against a recorded name that names itself.
    // An open instantiation of an alias names its arguments where it was written, which its own target is recorded
    // in terms of: its written arguments are read here instead, into the alias's target (below).
    auto open_alias = false;
    if (const auto written = t != nullptr ? t->LastTypePart()->Written() : nullptr; written != nullptr) {
      auto const *const named = HeadOf(written).Kind == Tag::Inst ? scope.SymbolOf(written) : nullptr;
      // Read here unless it holds an operation over a parameter bound here, which only folding can read: that is keyed
      // from its written arguments below.
      const auto read = named != nullptr and named->Alias == nullptr and named->Kind == TypeKind::Class
        and not named->IsConcrete ? scope.ReadIn(written) : nullptr;
      if (read != nullptr) {
        append_id(read);
        return out;
      }
      open_alias = named != nullptr and named->Alias != nullptr
        and (not ParamsOf(written).Types.empty() or not ParamsOf(written).Comps.empty());

      // An open instantiation of an alias stands for its target ("Scope::AliasTargetId"), recorded in the same terms as
      // its arguments: that target read here.
      if (open_alias) {
        if (const auto target = Scope::AliasTargetId(*named); target != nullptr
          and (HeadOf(target).Kind == Tag::Inst or HeadOf(target).Kind == Tag::Variant)) {
          if (const auto read = scope.ReadIn(target); read != nullptr) {
            append_id(read);
            return out;
          }
        }
      }
      if (named != nullptr and named->Alias != nullptr and not open_alias) {
        if (genex::contains(keying_aliases, named)) {
          key.PushPtr(named);
          return out;
        }
        keying_aliases.push_back(named);
        struct PopGuard { ~PopGuard() { keying_aliases.pop_back(); } } const _pop;
        if (const auto target = Scope::AliasTargetId(*named); target != nullptr) { append_id(target); }
        else { key.PushPtr(named); }
        return out;
      }
    }

    // Anything else is what it resolves to here: an alias its target, a binding what it is bound to, a parameter its
    // identity.
    // A name recording a parameter is that parameter's binding here, keyed as a binding (below); the lookup answers what
    // the binding links, which for one still waiting on an alias's target is only that target's template.
    auto const *sym = given;
    if (sym == nullptr and t != nullptr and not open_alias) {
      if (const auto written = t->LastTypePart()->Written(); written != nullptr and HeadOf(written).Kind == Tag::Param) {
        sym = scope.ResolveWritten(written);
      }
    }
    if (sym == nullptr and not open_alias) { sym = scope.GetTypeSymbol(t); }

    // A name nothing resolves (an instantiation not made, reached by spelling) is keyed by what it names: its arguments
    // recorded as the instantiation would record them ("RecordedArgsFor"), then an alias as its target with them
    // substituted, a class as its template and them.
    if (sym == nullptr and t != nullptr and not t->LastTypePart()->GnArgGroup->Args.IsEmpty()) {
      auto const *head = scope.GetTypeSymbol(t->WithoutGenerics()->WithoutConvention().get());
      // A "use" passes its arguments straight to what it names, so it is followed to that first: a class, or a "type"
      // alias ("use ..::SizedIntegerSigned" names the alias "SizedIntegerSigned[cmp w]").
      auto const *named = head;
      while (named != nullptr and named->Alias != nullptr and named->Alias->FromUseStmt) {
        auto const *const used = named->UseTarget();
        if (used == nullptr or used == named) { break; }
        named = used;
      }
      if (named != nullptr and named->Alias != nullptr and not named->Alias->FromUseStmt
        and not genex::contains(keying_aliases, named)) {
        // A "type" alias: its target ("Scope::AliasTargetId"), its parameters bound to the arguments.
        keying_aliases.push_back(named);
        struct PopHead { ~PopHead() { keying_aliases.pop_back(); } } const _pop;
        auto const &alias = *named->Alias;
        auto const *const decl = alias.Stmt != nullptr and alias.Stmt->GetAstScope() != nullptr
          ? alias.Stmt->GetAstScope()
          : alias.DeclScope;
        auto recorded = alias.Params != nullptr and decl != nullptr
          ? RecordedArgsFor(*t->LastTypePart()->GnArgGroup, *alias.Params, scope, *decl)
          : spp::Vec<spp::Unique<GenericArgumentAst>>();
        const auto target = Scope::AliasTargetId(*named);
        if (not recorded.IsEmpty() and target != nullptr) {
          const auto recorded_args = recorded | genex::views::ptr | genex::to<spp::Vec>();
          const auto bindings = spp::analyse::utils::type_resolution::BindArgs(*alias.Params, recorded_args, scope);
          if (const auto id = SubstituteTypeId(target, bindings); id != nullptr) {
            append_id(id);
            return out;
          }
        }
      }
      if (named != nullptr and named != head and named->Alias == nullptr) { head = named; }
      if (head != nullptr and head->Alias == nullptr and head->Type != nullptr and head->InstanceOf == nullptr) {
        // A variadic class (a variant, a tuple) has no defaults to record: its arguments are keyed as written.
        auto const &written_args = *t->LastTypePart()->GnArgGroup;
        if (head->Type->GnParamGroup->GetVariadicParams() != nullptr) {
          append_id(scope.InstanceTypeId(*head, written_args.GetAllArgs()));
          return out;
        }
        auto recorded = RecordedArgsFor(written_args, *head->Type->GnParamGroup, scope, *head->LinkedScope);
        if (not recorded.IsEmpty()) {
          append_id(scope.InstanceTypeId(*head, recorded | genex::views::ptr | genex::to<spp::Vec>()));
          return out;
        }
      }
    }
    if (sym == nullptr) {
      push_spelling(Tag::Unresolved, *t);
      return out;
    }
    if (sym->Alias != nullptr and sym->Alias->Resolved != nullptr) {
      if (genex::contains(keying_aliases, sym)) {
        key.PushPtr(sym);
        return out;
      }
      keying_aliases.push_back(sym);
      struct PopAlias { ~PopAlias() { keying_aliases.pop_back(); } } const _pop;
      if (const auto target = Scope::AliasTargetId(*sym); target != nullptr) { append_id(target); }
      else { push_spelling(Tag::Unresolved, *sym->Alias->Resolved); }
      return out;
    }
    if (sym->Kind == TypeKind::GenericArg) {
      // Held as the binding holds it ("T" bound to "&mut Str" is "&mut Str"), as "TypeRef::Of" reads it, unless written
      // with a convention of its own.
      if (sym->Convention != nullptr and (t == nullptr or t->GetConvention() == nullptr)) {
        key.Push(Tag::Conv, static_cast<std::uint64_t>(sym->Convention->Tag()));
      }

      // Bound to an alias whose target is not made yet ("U8", before its "SizedInteger" instance is), it links the
      // target's template; the alias keys as its target does, by the arguments it records, with nothing made.
      sym->FollowBoundAlias();
      if (sym->BoundAlias != nullptr) {
        auto inner = KeyIn(scope, nullptr, sym->BoundAlias);
        key.AppendKey(inner.Key);
        out.Members = std::move(inner.Members);
        return out;
      }
      const auto bound = sym->AsBoundSymbol();

      // Nothing is bound to a bare generic template: linking one means the instance its value names is not made yet.
      // The value's recorded identity, where closed, is that instance.
      if (bound != sym and bound->IsBareTemplate() and sym->GenericVal != nullptr) {
        if (const auto written = sym->GenericVal->LastTypePart()->Written();
          written != nullptr and not written->HasSelf and not written->HasUnresolved
          and (HeadOf(written).Kind == Tag::Inst or HeadOf(written).Kind == Tag::Variant)
          and ParamsOf(written).Types.empty() and ParamsOf(written).Comps.empty()) {
          append_id(written);
          return out;
        }
      }
      if (bound == sym) {
        key.Push(Tag::Bound, static_cast<std::uint64_t>(sym->BindsParamId));
        if (sym->GenericVal != nullptr) { push_spelling(Tag::Bound, *sym->GenericVal); }
        else { key.PushText(Tag::Bound, sym->Name->ToView()); }
        return out;
      }
      sym = bound;
    }
    if (sym->Kind == TypeKind::GenericParam) {
      key.Push(Tag::Param, static_cast<std::uint64_t>(sym->ParamId));
      return out;
    }
    if (sym->Kind == TypeKind::Self) {
      key.Push(Tag::Self);
      return out;
    }
    if (sym->Kind == TypeKind::Class and sym->InstanceOf != nullptr) {
      instance_key(*sym);
      return out;
    }
    key.PushPtr(sym);
    return out;
  }

  auto TypeKeyIn(spp::analyse::scopes::Scope const &scope, spp::asts::TypeAst const &t) -> TypeKeyParts {
    return KeyIn(scope, &t, nullptr);
  }

  /// A variant, by the set of the members its "Variants" argument lists, read in "read_in": flattened, deduplicated,
  /// and in one order, whatever order they were written or substituted in.
  auto VariantKey(
    spp::analyse::scopes::TypeSymbol const &tmpl,
    spp::Vec<spp::asts::TypeAst const*> const &members,
    spp::analyse::scopes::Scope const &read_in)
    -> TypeKeyParts {
    using namespace spp::analyse::scopes;
    auto out = TypeKeyParts();
    for (auto const *member : members) {
      auto parts = TypeKeyIn(read_in, *member);
      auto flat = parts.Members.IsEmpty() ? spp::Vec<InstanceKey>{std::move(parts.Key)} : std::move(parts.Members);
      for (auto &m : flat) {
        if (not genex::contains(out.Members, m)) { out.Members.EmplaceBack(std::move(m)); }
      }
    }
    std::ranges::sort(out.Members, [](auto const &a, auto const &b) { return a < b; });
    out.Key.Push(InstanceKey::Tag::Variant);
    out.Key.PushPtr(&tmpl);
    for (auto const &m : out.Members) { PushTypePart(out.Key, InstanceKey(m)); }
    return out;
  }
}

auto Scope::InstanceTypeId(
  TypeSymbol const &tmpl, Vec<GenericArgumentAst*> const &args, GenericParameterGroupAst const *params) const
  -> TypeId {
  // A variant is the set of its members; anything else its template and its arguments - an alias of a variant too,
  // whose own arguments are not its members ("Opt[T]").
  if (tmpl.Alias == nullptr and utils::type_predicates::IsTypeVariant(tmpl, *this)) {
    // The members: the tuple named "Variants" (the analysed form), a lone tuple, or - written "A or B" before it is
    // analysed - the positional arguments themselves.
    auto const *variants = static_cast<GenericArgumentAst const*>(nullptr);
    for (auto const *arg : args) { if (arg->Name != nullptr and arg->ViewName() == "Variants") { variants = arg; } }
    if (variants == nullptr and args.Len() == 1 and args[0]->Name == nullptr) { variants = args[0]; }
    auto members = Vec<TypeAst const*>();
    if (variants != nullptr and variants->TypeVal != nullptr) {
      for (auto const *member : variants->TypeVal->LastTypePart()->GnArgGroup->GetTypeArgs()) { members.EmplaceBack(member->TypeVal.get()); }
    }
    else {
      for (auto const *arg : args) { if (arg->Name == nullptr and arg->TypeVal != nullptr) { members.EmplaceBack(arg->TypeVal.get()); } }
    }
    return InternTypeKey(VariantKey(tmpl, members, *this).Key);
  }
  auto key = InstanceKey();
  key.Push(InstanceKey::Tag::Inst);
  key.PushPtr(&tmpl);
  key.PushKey(*InstanceIdentityKey(args, params));
  return InternTypeKey(std::move(key));
}

auto Scope::TypeKey(
  TypeAst const &type) const
  -> InstanceKey {
  return TypeKeyIn(*this, type).Key;
}

auto Scope::TypeIdOfSym(
  TypeSymbol const &sym,
  const std::uint64_t conv) const
  -> TypeId {
  auto key = InstanceKey();
  if (conv != 0) { key.Push(InstanceKey::Tag::Conv, conv); }
  key.AppendKey(KeyIn(*this, nullptr, &sym).Key);
  return key.HasUnresolved ? nullptr : InternTypeKey(std::move(key));
}

auto Scope::FileInstance(
  const TypeId id, TypeSymbol &instance)
  -> void {
  auto *const tmpl = const_cast<TypeSymbol*>(static_cast<TypeSymbol const*>(HeadOf(id).Ptr));
  if (tmpl != nullptr) { tmpl->Instances[BareTypeId(id)] = &instance; }
}

auto Scope::SymbolOf(
  const TypeId id) const
  -> TypeSymbol* {
  using generate::common_types_precompiled::SELF_TYPE;
  using Tag = InstanceKey::Tag;
  if (id == nullptr or id->HasUnresolved) { return nullptr; }

  auto const &head = HeadOf(id);
  auto *const ptr = const_cast<TypeSymbol*>(static_cast<TypeSymbol const*>(head.Ptr));
  switch (head.Kind) {
  case Tag::Sym:
    return ptr;
  case Tag::Param:
    return GenericParamOf(head.ParamId);
  case Tag::Inst:
  case Tag::Variant: {
    const auto hit = ptr->Instances.find(BareTypeId(id));
    return hit != ptr->Instances.end() ? hit->second : nullptr;
  }
  case Tag::Self: {
    auto *const self = GetTypeSymbol(SELF_TYPE.get());
    return self != nullptr ? self->AsClassSymbol() : nullptr;
  }
  default:
    return nullptr;
  }
}

auto Scope::ReadIn(
  const TypeId written) const
  -> TypeId {
  if (written == nullptr) { return nullptr; }
  auto const &params = ParamsOf(written);
  if (params.Types.empty() and params.Comps.empty()) { return written; }

  auto subst = TypeSubst();
  for (const auto pid : params.Types) {
    auto *const param = GenericParamOf(pid);
    auto *const binding = param != nullptr ? Canon(*param) : nullptr;
    if (binding == nullptr or binding == param) { continue; }
    // Held as the binding holds it ("Splice" keeps the parameter's own convention where it is written with one).
    const auto bound = TypeIdOfSym(*binding, 0);
    if (bound == nullptr) { continue; }
    auto const &head = HeadOf(bound);
    const auto is_itself = (head.Kind == InstanceKey::Tag::Param or head.Kind == InstanceKey::Tag::Bound)
      and head.ParamId == pid;
    if (is_itself) { continue; }
    subst.Types.emplace_back(pid, bound);
    if (param->IsVariadic) { subst.TypePacks.push_back(pid); }
  }
  for (const auto comp : params.Comps) {
    auto *const param = GenericCompParamOf(CompParamIdOfText(comp));
    auto *const binding = param != nullptr ? CanonVar(*param) : nullptr;
    if (binding == nullptr or binding == param) { continue; }
    // Keyed from the binding's symbol, as a type binding is ("TypeIdOfSym"), not its name spelled again here, where a
    // nearer symbol of that name can answer instead.
    auto value = Str();
    utils::comp_generics::CompIdentityOfSym(*binding, *this, value);
    if (auto const *const bound_value = binding->BoundCompValue(); bound_value != nullptr) {
      NoteCompValue(value, *bound_value);
    }
    const auto bound = static_cast<std::uint64_t>(spp::utils::Intern(value));
    if (bound == comp) { continue; }
    subst.Comps.emplace_back(comp, bound);
    if (param->IsVariadic) { subst.CompPacks.push_back(comp); }
  }
  return SubstituteTypeId(written, subst);
}

auto Scope::WrittenIdOf(
  TypeSymbol const &sym)
  -> TypeId {
  using Tag = InstanceKey::Tag;
  auto key = InstanceKey();
  if (sym.Kind == TypeKind::GenericParam and sym.ParamId != 0) {
    key.Push(Tag::Param, sym.ParamId);
    return InternTypeKey(std::move(key));
  }
  if (sym.Kind == TypeKind::GenericArg and sym.BindsParamId != 0) {
    key.Push(Tag::Param, sym.BindsParamId);
    return InternTypeKey(std::move(key));
  }
  if (sym.Kind == TypeKind::Self) {
    key.Push(Tag::Self);
    return InternTypeKey(std::move(key));
  }
  // An instantiation (of a class or of an alias) is the identity it is filed under; anything else - a class, a
  // template, an alias - is itself.
  if (sym.InstanceOf != nullptr and sym.Id != nullptr) { return sym.Id; }
  key.PushPtr(&sym);
  return InternTypeKey(std::move(key));
}

auto Scope::WrittenIdOfCompParam(
  const std::uint64_t param_id)
  -> TypeId {
  auto key = InstanceKey();
  key.Push(InstanceKey::Tag::Comp, CompParamText(param_id));
  return InternTypeKey(std::move(key));
}

auto Scope::ResolveWritten(
  const TypeId written) const
  -> TypeSymbol* {
  using Tag = InstanceKey::Tag;
  if (written == nullptr) { return nullptr; }
  auto const &head = HeadOf(written);
  switch (head.Kind) {
  case Tag::Param: {
    auto *const param = GenericParamOf(head.ParamId);
    return param != nullptr ? Canon(*param) : nullptr;
  }
  case Tag::Sym: {
    // A class, template, alias or mock names itself from anywhere; "Canon" has nothing to re-read for most of them.
    auto *const sym = const_cast<TypeSymbol*>(static_cast<TypeSymbol const*>(head.Ptr));
    auto *const canon = Canon(*sym);
    return canon != nullptr ? canon : sym;
  }
  case Tag::Inst:
  case Tag::Variant:
    return SymbolOf(ReadIn(written));
  default:
    return nullptr;
  }
}

namespace {
  /// The value each recorded comp identity stands for ("Scope::NoteCompValue"). Keys are interned for the process, and
  /// the values are clones owned here, so entries outlive any one compilation's scopes.
  auto CompValues() -> spp::Map<std::uint64_t, spp::Shared<spp::asts::ExpressionAst>>& {
    static auto values = spp::Map<std::uint64_t, spp::Shared<spp::asts::ExpressionAst>>();
    return values;
  }
}

auto Scope::NoteCompValue(
  const StrView identity, ExpressionAst const &value)
  -> void {
  const auto key = static_cast<std::uint64_t>(spp::utils::Intern(identity));
  if (not CompValues().contains(key)) { CompValues().emplace(key, AstCloneShared(&value)); }
}

auto Scope::AliasTargetId(
  TypeSymbol const &alias)
  -> TypeId {
  if (alias.Alias == nullptr or alias.Alias->Resolved == nullptr) { return nullptr; }
  auto const &resolved = *alias.Alias->Resolved;
  if (const auto recorded = resolved.LastTypePart()->Written(); recorded != nullptr) { return recorded; }
  auto const *const stmt = alias.Alias->Stmt;
  auto const *const where = stmt != nullptr and stmt->GetAstScope() != nullptr ? stmt->GetAstScope() : alias.Alias->DeclScope;
  return where != nullptr ? where->TypeIdOf(resolved) : nullptr;
}

auto Scope::CompValueOf(
  const std::uint64_t identity)
  -> Unique<ExpressionAst> {
  // Recorded whole ("NoteCompValue"), or a pack rebuilt from its elements, as a substitution makes one.
  if (const auto hit = CompValues().find(identity); hit != CompValues().end()) { return AstClone(hit->second.get()); }
  const auto elems = CompPackElements(spp::utils::InternedText(static_cast<spp::utils::InternedId>(identity)));
  if (not elems.has_value()) { return nullptr; }
  auto values = Vec<Unique<ExpressionAst>>();
  for (auto const elem : *elems) {
    auto value = CompValueOf(static_cast<std::uint64_t>(spp::utils::Intern(elem)));
    if (value == nullptr) { return nullptr; }
    values.EmplaceBack(std::move(value));
  }
  return MakeUnique<TupleLiteralAst>(nullptr, std::move(values), nullptr);
}

auto Scope::TypeAstOf(
  const TypeId id) const
  -> Shared<TypeAst> {
  using generate::common_types_precompiled::SELF_TYPE;
  using Tag = InstanceKey::Tag;
  if (id == nullptr) { return nullptr; }
  auto const &head = HeadOf(id);
  const auto bare = BareTypeId(id);

  // An instantiation or a variant is built from its identity, part by part, each part recording its own: a made one's
  // name is spelled as it was written where it was made, which it need not mean anywhere else, nested parts included.
  auto out = Shared<TypeAst>(nullptr);
  auto *const sym = bare->HasSelf or head.Kind == Tag::Inst or head.Kind == Tag::Variant ? nullptr : SymbolOf(bare);
  if (sym != nullptr) { out = sym->FqName(); }
  else {
    switch (head.Kind) {
    case Tag::Param:
    case Tag::Bound: {
      // Stamped, so it is that parameter wherever it is read, not whatever shares its spelling there. A binding with no
      // type of its own to name is its parameter too, read through the binding wherever that is in view.
      auto const *const param = GenericParamOf(head.ParamId);
      if (param == nullptr) { return nullptr; }
      out = AstCloneShared(param->FqName().get());
      out->LastTypePart()->SetWritten(head.Kind == Tag::Param ? bare : WrittenIdOf(*param));
      break;
    }
    case Tag::Self:
      out = AstCloneShared(SELF_TYPE.get());
      break;
    case Tag::Sym:
      out = static_cast<TypeSymbol const*>(head.Ptr)->FqName();
      break;
    case Tag::Inst: {
      auto const *const tmpl = static_cast<TypeSymbol const*>(head.Ptr);
      auto group = GenericArgumentGroupAst::NewEmpty();
      for (auto const &arg : ArgsOf(head.Args)) {
        auto name = arg.Named
          ? TypeIdentifierAst::FromString(Str(spp::utils::InternedText(static_cast<spp::utils::InternedId>(arg.Name))))
          : nullptr;
        if (arg.Type != nullptr) {
          auto type = TypeAstOf(arg.Type);
          if (type == nullptr) { return nullptr; }
          group->Args.EmplaceBack(GenericArgumentAst::NewType(std::move(name), std::move(type)));
          continue;
        }
        auto value = CompValueOf(arg.Comp);
        if (value == nullptr) { return nullptr; }
        group->Args.EmplaceBack(GenericArgumentAst::NewComp(std::move(name), std::move(value)));
      }
      // Headed by the template it instantiates, recorded so the head resolves to it from anywhere. An alias by its own
      // name: its qualified name is its target's ("Var[..]"), which the alias's arguments do not name.
      auto head = tmpl->Alias != nullptr ? AstCloneShared(tmpl->Name.get()) : tmpl->FqName()->WithoutGenerics();
      out = head->WithGenerics(std::move(group));
      out->LastTypePart()->SetTemplateWritten(WrittenIdOf(*tmpl));
      if (not bare->HasSelf and not bare->HasUnresolved) { out->LastTypePart()->SetWritten(bare); }
      break;
    }
    case Tag::Variant: {
      // In the order an analysed variant lists them ("TypeIdentifierAst::Stage7_AnalyseSemantics"), by name, which a
      // pattern over it is matched in.
      auto keyed = Vec<Pair<Str, Shared<TypeAst>>>();
      for (const auto member : head.Members) {
        auto type = TypeAstOf(member);
        if (type == nullptr) { return nullptr; }
        auto const *const member_sym = SymbolOf(BareTypeId(member));
        keyed.EmplaceBack(member_sym != nullptr ? member_sym->FqName()->ToString() : type->ToString(), std::move(type));
      }
      std::ranges::stable_sort(keyed, {}, [](auto const &k) -> Str const& { return k.first; });
      auto members = Vec<Shared<TypeAst>>();
      for (auto &[_, type] : keyed) { members.EmplaceBack(std::move(type)); }
      // In the analysed form, which holds the members as one tuple ("Var[Variants=Tup[..]]").
      out = generate::common_types::VariantType(0, {});
      out->LastTypePart()->GnArgGroup->Args.EmplaceBack(GenericArgumentAst::NewType(
        TypeIdentifierAst::FromString("Variants"), generate::common_types::TupleType(0, std::move(members))));
      if (not bare->HasSelf and not bare->HasUnresolved) {
        out->LastTypePart()->SetWritten(bare);
        // The tuple of members is a type in its own right, keyed from its (recorded) members, so it too means the same
        // wherever it is read.
        if (auto const *const variants = out->LastTypePart()->GnArgGroup->At("Variants"); variants != nullptr) {
          if (const auto tup_id = TypeIdOf(*variants->TypeVal); tup_id != nullptr) {
            variants->TypeVal->LastTypePart()->SetWritten(tup_id);
          }
        }
      }
      break;
    }
    default:
      return nullptr;
    }
  }
  if (out == nullptr or head.Conv == 0) { return out; }
  if (head.Conv == static_cast<std::uint64_t>(ConventionTag::MUT)) {
    return out->WithConvention(MakeUnique<ConventionMutAst>(nullptr, nullptr));
  }
  return out->WithConvention(MakeUnique<ConventionRefAst>(nullptr));
}

auto Scope::TypeIdOf(
  TypeAst const &type) const
  -> TypeId {
  auto key = TypeKey(type);
  return key.HasUnresolved ? nullptr : InternTypeKey(std::move(key));
}

auto Scope::GetNsSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const
  -> NamespaceSymbol* {
  // Get the symbol from the symbol table if it exists.
  if (sym_name == nullptr) { return nullptr; }
  const auto scope = this;
  auto sym = InternalTable.NsTbl.Get(sym_name);

  // If the symbol doesn't exist, and this is a non-exclusive
  // search, check the parent scope.
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->GetNsSymbol(sym_name, exclusive);
  }

  // Return the found symbol, or nullptr.
  return sym;
}

auto Scope::GetVarSymbolOutermost(
  Ast const &expr) const -> Pair<VariableSymbol*, Scope const*> {
  // Define helper methods to check expression types.
  auto is_valid_postfix_expression = []<typename OpType>(const auto ast) -> bool {
    auto postfix_expr = ast->template To<PostfixExpressionAst>();
    if (postfix_expr == nullptr) { return false; }
    auto postfix_op = postfix_expr->Op->template To<OpType>();
    return postfix_op != nullptr;
  };

  auto is_valid_postfix_expression_runtime = [&](const auto ast) -> bool {
    return is_valid_postfix_expression.operator()<
      PostfixExpressionOperatorRuntimeMemberAccessAst>(ast);
  };

  auto is_valid_postfix_expression_static = [&](const auto ast) -> bool {
    return is_valid_postfix_expression.operator()<
      PostfixExpressionOperatorStaticMemberAccessAst>(ast);
  };

  auto is_valid_postfix_expression_deref = [&](const auto ast) -> bool {
    return is_valid_postfix_expression.operator()<
      PostfixExpressionOperatorDerefAst>(ast);
  };

  auto adjusted_name = &expr;
  if (is_valid_postfix_expression_runtime(&expr)) {
    // Keep moving into the left-hand-side until there is
    // no left-hand-side: "a.b.c" becomes "a".
    while (is_valid_postfix_expression_runtime(adjusted_name) or is_valid_postfix_expression_deref(adjusted_name)) {
      adjusted_name = adjusted_name->To<PostfixExpressionAst>()->Lhs.get();
    }

    // Get the symbol (will be in this scope), and return
    // it with the scope.
    auto sym = GetVarSymbol(adjusted_name->To<IdentifierAst>());
    return {sym, this};
  }

  if (is_valid_postfix_expression_static(&expr)) {
    // This is possible with a left-hand-side type or
    // namespace.
    const auto postfix_expr = expr.ToUnchecked<PostfixExpressionAst>();
    const auto postfix_op = postfix_expr->Op->ToUnchecked<PostfixExpressionOperatorStaticMemberAccessAst>();

    // Type based left-hand-side, such as
    // "some_namespace::Type::static_member()"
    if (const auto type_lhs = postfix_expr->Lhs->To<TypeAst>()) {
      const auto type_sym = GetTypeSymbol(type_lhs);
      const auto var_sym = type_sym->LinkedScope->GetVarSymbol(postfix_op->Name.get());
      return {var_sym, const_cast<Scope const*>(type_sym->LinkedScope)};
    }

    // Namespace based left-hand-side, such as
    // "a::b::c::my_function()"
    auto namespace_scope = this;
    if (is_valid_postfix_expression_static(adjusted_name)) {
      adjusted_name = adjusted_name->To<PostfixExpressionAst>()->Lhs.get();
      namespace_scope = namespace_scope->ConvertPostfixToNestedScope(adjusted_name->To<ExpressionAst>());
    }
    auto sym = namespace_scope ? namespace_scope->GetVarSymbol(postfix_op->Name.get()) : nullptr;
    return {sym, namespace_scope};
  }

  // Identifiers or non-symbolic expressions can use the
  // normal lookup.
  auto sym = GetVarSymbol(adjusted_name->To<IdentifierAst>());
  return {sym, this};
}

auto Scope::DepthDiff(
  const Scope *scope) const -> sys::ssize_t {
  // Create an internal function to call recursively with
  // a counter.
  auto func = [](this auto &&self, const Scope *source, const Scope *target, const sys::ssize_t depth) -> sys::ssize_t {
    if (source == target) { return depth; }
    for (auto const *sup_scope : source->DirectSupScopes) {
      if (const auto result = self(sup_scope, target, depth + 1z); result >= 0z) {
        return result;
      }
    }
    return -1;
  };

  return func(this, scope, 0z);
}

auto Scope::FinalChildScope() const -> Scope const* {
  // If there are no children, return this scope (base case
  // for the recursion). Otherwise, return the final child
  // scope (recursively searching).
  return Children.IsEmpty()
    ? this
    : Children.Back()->FinalChildScope();
}

auto Scope::Ancestors() const -> Vec<Scope const*> {
  // Get all ancestor scopes, including this scope, and the
  // global scope.
  auto scopes = Vec<Scope const*>();
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    scopes.EmplaceBack(scope);
  }
  return scopes;
}

auto Scope::ParentModule() const -> Scope* {
  // Get the parent module scope, if it exists.
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    if (std::holds_alternative<ScopeIdentifierName>(scope->Name)) {
      return const_cast<Scope*>(scope); // TODO: REMOVE CONST CAST
    }
  }
  return nullptr;
}

auto Scope::TopLevelParentModule() const -> Scope* {
  // Get the top level parent module scope (ie until the
  // parent is the global scope).
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    const auto next_scope = scope->Parent;
    if (std::holds_alternative<ScopeBlockName>(next_scope->Name) and std::get<ScopeBlockName>(next_scope->Name).Name.
      contains("__global__")) {
      return const_cast<Scope*>(scope); // TODO: REMOVE CONST CAST
    }
  }
  return nullptr;
}

auto Scope::GetEnclosingTypeScope(
  CompilerMetaData const &meta) const -> Scope* {
  // If the current scope is a lambda scope, use the original
  // scope that it overrode.
  if (const auto block_name = std::get_if<ScopeBlockName>(&Name)) {
    if (block_name != nullptr and block_name->Name.contains("<closure-inner")) {
      return meta.OverriddenScopeForClosure->GetEnclosingTypeScope(meta);
    }
  }

  // Walk up the scope chain. Return the first type scope that's
  // non-compiler-generated, or the LinkedScope of a "Self" type
  // symbol found in a sup-block scope (for module-level sup
  // blocks that have no type scope in their chain).
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    if (scope->TySym != nullptr and not scope->TySym->IsMock()) {
      return const_cast<Scope*>(scope);
    }

    for (auto const &ty_sym : scope->InternalTable.TypeTbl.All()) {
      if (ty_sym->IsSelf() and ty_sym->LinkedScope != nullptr) {
        return ty_sym->LinkedScope;
      }
    }
  }
  return nullptr;
}

auto Scope::GetEnclosingSelfType(
  CompilerMetaData const &meta) const -> Shared<TypeAst> {
  // If we are already in a module scope, there is no self
  // type.
  auto current_scope = this;
  if (std::holds_alternative<ScopeIdentifierName>(current_scope->Name)) {
    return nullptr;
  }

  // Escape closure scopes for the Self type. Prefer using
  // the "Self" symbol type first.
  if (current_scope->NameAsString().starts_with("<closure-outer")) {
    current_scope = meta.OverriddenScopeForClosure;
  }
  const auto self_name = MakeUnique<TypeIdentifierAst>(0uz, "Self", nullptr);
  if (const auto self_sym = current_scope->GetTypeSymbol(self_name.get());
    self_sym != nullptr and self_sym->LinkedScope != nullptr and self_sym->LinkedScope->TySym != nullptr) {
    // Inside a template, "Self" is the template over its own
    // parameters ("TypeSymbol::GenericSelfName").
    return self_sym->LinkedScope->TySym->GenericSelfName();
  }

  // Use a "seen" walker to prevent scope searching cycles due
  // to nested closures, whose inner/outer scopes don't follow
  // normal scoping hierarchies.
  auto seen = Set<Scope const*>();
  while (true) {
    if (not seen.insert(current_scope).second) { return nullptr; }

    // Escape closure scopes for the Self type.
    if (current_scope->NameAsString().starts_with("<closure-outer")
      and meta.OverriddenScopeForClosure != nullptr) {
      current_scope = meta.OverriddenScopeForClosure;
      continue;
    }

    // Nothing above this scope, so nothing encloses it.
    if (current_scope->Parent == nullptr) { return nullptr; }

    // Only get a scope right under the module scope.
    if (not std::holds_alternative<ScopeIdentifierName>(current_scope->Parent->Name)) {
      current_scope = current_scope->Parent;
      continue;
    }

    // The walk lands on whatever ast owns the scope under
    // the module, which is not always one that declares a
    // type - a module-level type statement or function owns
    // one too, and neither encloses a "Self".
    return current_scope->AstNode != nullptr
      ? AstNameOrNull(current_scope->AstNode)
      : nullptr;
  }
  return nullptr;
}

auto Scope::SupScopes() const
  -> Vec<Scope*> const& {
  // Get all super scopes, recursively, yielding each one once.
  // Use the cache if available, saving on a massive number of
  // allocations.
  const auto generation = TypeStructureGeneration();
  if (_SupScopesGen == generation) {
    return _SupScopesCache;
  }

  // The function for walking scopes; it involves walking the
  // direct sup-scopes of a type, and then recursively walking
  // the sup-scopes' sup-scopes, etc.
  auto scopes = Vec<Scope*>();
  auto seen = Set<Scope const*>();
  const auto walk = [&](auto const &self, Scope const &from) -> void {
    if (OnSupScopesRead) { OnSupScopesRead(from); }
    for (const auto sup_scope : from.DirectSupScopes) {
      if (not seen.insert(sup_scope).second) { continue; }
      scopes.push_back(sup_scope);
      self(self, *sup_scope);
    }
  };
  walk(walk, *this);

  // Cache the result given we didn't use the cache if we reach
  // this point. Set the generation and return the reference to
  // the cache (no copy).
  _SupScopesCache = std::move(scopes);
  _SupScopesGen = TypeStructureGeneration() == generation ? generation : 0;
  return _SupScopesCache;
}

auto Scope::SupTypes() const -> Vec<Shared<TypeAst>> {
  // Get all the sup-scopes (cls/sup scopes) superimposed over
  // a type, filter to keep the class ones, and resolve (extract)
  // names properly. Collect and return. This uses "SupScopes" so
  // utilises an internal cache.
  auto ts = SupScopes()
    | genex::views::filter([](const auto scope) { return AstAs<ClassPrototypeAst>(scope->AstNode); })
    | genex::views::transform(ResolveSupTypeName)
    | genex::views::filter([](auto const &type) { return type != nullptr; }) // Todo: shouldn't need.
    | genex::to<Vec>();
  return ts;
}

auto Scope::ConvertPostfixToNestedScope(
  ExpressionAst const *postfix_ast) const -> Scope const* {
  // Get the left-hand-side namespace's member's type.
  if (const auto lhs_as_ident = postfix_ast->To<IdentifierAst>()) {
    const auto ns_sym = GetNsSymbol(lhs_as_ident);
    return ns_sym ? ns_sym->LinkedScope : nullptr;
  }

  // Postfix lhs -> get the ns scopes.
  auto lhs = postfix_ast;
  auto namespaces = Vec<IdentifierAst*>();
  while (auto const *postfix_lhs = lhs->To<PostfixExpressionAst>()) {
    const auto op = postfix_lhs->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>();
    namespaces.EmplaceBack(op->Name->To<IdentifierAst>());
    lhs = postfix_lhs->Lhs.get();
  }
  if (const auto lhs_as_ident = lhs->To<IdentifierAst>()) {
    // todo: is the condition required? just body.
    namespaces.EmplaceBack(const_cast<IdentifierAst*>(lhs_as_ident));
  }

  auto scope = this;
  for (auto const *ns : namespaces | genex::views::reverse) {
    const auto ns_sym = scope->GetNsSymbol(ns);
    scope = ns_sym ? ns_sym->LinkedScope : nullptr;
    if (scope == nullptr) { break; }
  }
  return scope;
}

auto Scope::NameAsString() const -> Str {
  // Identifier based scope name.
  if (std::holds_alternative<ScopeIdentifierName>(Name)) {
    auto const name_as_id = std::get<ScopeIdentifierName>(Name).Name;
    return name_as_id->ToString();
  }

  // TypeIdentifier based scope name.
  if (std::holds_alternative<ScopeTypeIdentifierName>(Name)) {
    auto const name_as_type = std::get<ScopeTypeIdentifierName>(Name).Name;
    return name_as_type->ToString();
  }

  // Block name.
  auto const name_as_block_name = std::get<ScopeBlockName>(Name).Name;
  return name_as_block_name;
}

auto Scope::FixChildrenToParentPointer() -> void {
  // Iterate all children, setting their parent pointer to
  // this scope. Recurse into them. This runs over a scope
  // tree that has already been read from, so anything cached
  // against the old shape of it has to go.
  BumpScopeLinkageGeneration();
  for (auto const &child : Children) {
    child->Parent = this;
    child->FixChildrenToParentPointer();
  }
}

SPP_MOD_END

auto spp::analyse::scopes::ScopeLinkageGeneration()
  -> std::uint64_t {
  return _ScopeLinkageGeneration;
}

auto spp::analyse::scopes::BumpScopeLinkageGeneration()
  -> void {
  // A scope moving in the tree changes the method set, so this
  // is the coarser of the two.
  ++_ScopeLinkageGeneration;
  ++_TypeStructureGeneration;
}

auto spp::analyse::scopes::TypeStructureGeneration()
  -> std::uint64_t {
  return _TypeStructureGeneration;
}

auto spp::analyse::scopes::BumpTypeStructureGeneration()
  -> void {
  ++_TypeStructureGeneration;
}
