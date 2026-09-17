module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbol_table;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.cmp_utils;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
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
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.compiler.module_tree;
import spp.utils.algorithms;
import spp.utils.error_formatter;
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

    /// The binding a scope gives a generic parameter, found by
    /// the parameter's identity rather than its spelling. Bindings
    /// are registered under the parameter's name, so each scope
    /// is asked by name ("get"), but only an answer carrying the
    /// parameter's own id counts: a callee's parameter spelled
    /// like a caller's is a different parameter, and is skipped
    /// rather than taken. Inside a "sup" block, the class's own
    /// parameters are bound by the block's "Self" instantiation
    /// ("Vec[T, A]" over the block's own "T" and "A"), whose
    /// bindings are on that instantiation's scope rather than on
    /// any ancestor, so that scope is asked too, then the super
    /// scopes ("sup_search"). Nothing binding it here leaves the
    /// parameter itself.
    template <typename Sym, typename Get, typename SupSearch>
    auto FindParamBinding(
      Scope const &from, Sym &param, const std::uint64_t id, Get const &get,
      SupSearch const &sup_search) -> Sym* {
      // Match when either the param id or the "binds" param id
      // on the type symbol match the incoming id.
      const auto is_it = [id](Sym const *found) {
        return found != nullptr and (found->ParamId == id or found->BindsParamId == id);
      };

      // Special case for "Self".
      static const auto self_name = TypeIdentifierAst::FromString("Self");

      // Iterate from the "from" scope upwards through its
      // ancestors.
      for (auto scope = &from; scope != nullptr; scope = scope->Parent) {
        if (const auto found = get(*scope, param.Name.get()); is_it(found)) { return found; }
        if (const auto self_sym = scope->InternalTable.TypeTbl.Get(self_name.get());
          self_sym != nullptr and self_sym->LinkedScope != nullptr and self_sym->LinkedScope != scope) {
          if (const auto found = get(*self_sym->LinkedScope, param.Name.get()); is_it(found)) { return found; }
        }
      }

      // Not found in the scope, so check through the sup scopes.
      // Todo: Do we want to allow for this?
      if (const auto found = sup_search(from, param.Name.get()); is_it(found)) { return found; }
      return &param;
    }

    /// The 3 type lookup caches, starting at "1" because "0"
    /// means "never computed" rather than "computed before
    /// anything moved".
    std::uint64_t _ScopeLinkageGeneration = 1;
    std::uint64_t _TypeStructureGeneration = 1;
    std::uint64_t _TypeLookupGeneration = 1;

    /// Bump the type lookup generation, when symbols change
    /// or scopes are re-pointed etc. Internal to this module.
    auto BumpTypeLookupGeneration() -> void {
      ++_TypeLookupGeneration;
    }
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

Scope::~Scope() {
  // A scope freed mid-compile (Stage 9.5) can have its address
  // reused by a new one, which must not inherit its cached lookups.
  BumpTypeLookupGeneration();
}

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

    for (auto const &t : all_type_syms) {
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
      {existing_sym->ScopeDefinedIn ? : this, sym->ScopeDefinedIn ? : this},
      ERR_ARGS(*existing_sym->Name, *sym->Name, "comptime variable identifier"));
  }

  // Add a variable symbol to the corresponding symbol table.
  InternalTable.VarTbl.Add(sym->Name.get(), sym);
}

auto Scope::AddTypeSymbol(
  Shared<TypeSymbol> const &sym)
  -> void {
  // Add a type symbol to the corresponding symbol table.
  BumpTypeLookupGeneration();
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
  BumpTypeLookupGeneration();
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
  BumpTypeLookupGeneration();
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

  // A name stamped with the comp parameter it named where
  // it was written means that parameter from here too, rather
  // than whatever its spelling names in this scope.
  if (not exclusive) {
    if (const auto stamp = sym_name->Stamp(); stamp != nullptr) {
      if (const auto canon = CanonVar(*stamp); canon != nullptr) { return canon; }
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

  // Check the cache, based on the generation and the scope,
  // before anything else, saving on a range of instructions.
  const auto generation = TypeLookupGeneration();
  const auto cacheable = not exclusive and sup_scope_search;
  if (auto cached = static_cast<TypeSymbol*>(nullptr);
    cacheable and sym_name->TryCachedLookup(this, generation, cached)) {
    return cached;
  }

  // A type stamped with the symbol it resolved to where it
  // was written means that symbol from here too, rather than
  // whatever its spelling happens to name in this scope.
  if (not exclusive) {
    if (const auto stamp = sym_name->Stamp(); stamp != nullptr) {
      if (const auto canon = Canon(*stamp); canon != nullptr) {
        if (cacheable) { sym_name->RememberLookup(this, generation, canon); }
        return canon;
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
  // may carry a stamp of its own.
  if (not exclusive and sym_name_extracted != sym_name) {
    if (const auto stamp = sym_name_extracted->Stamp(); stamp != nullptr) {
      if (const auto canon = Canon(*stamp); canon != nullptr) {
        if (cacheable) { sym_name->RememberLookup(this, generation, canon); }
        return canon;
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
        InstanceIdentityKey(sym_name_extracted->GnArgGroup->GetAllArgs(), params));

      if (hit != tmpl->Instances.end()) {
        if (cacheable) { sym_name->RememberLookup(this, generation, hit->second); }
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

      if (sym->IdentityKey != InstanceIdentityKey(sym_name_extracted->GnArgGroup->GetAllArgs(), params)) {
        sym = nullptr;
      }
    }
  }

  // Remember the answer against this scope, so the next
  // ask of the same question is a pointer comparison.
  if (cacheable) { sym_name->RememberLookup(this, generation, sym); }

  // Return the found symbol, or nullptr.
  return sym;
}

auto Scope::CanonVar(VariableSymbol &sym) const -> VariableSymbol* {
  // Only a generic comp parameter can be canonical-ised.
  if (sym.Kind != VariableKind::GenericCompParam or sym.ParamId == 0) { return nullptr; }

  // Apply the logic with this scope set as the scope we
  // are searching from.
  return FindParamBinding(
    *this, sym, sym.ParamId,
    [](Scope const &scope, auto const *name) { return scope.InternalTable.VarTbl.Get(name); },
    [](Scope const &scope, auto const *name) { return SearchSupScopesForVar(scope, name); });
}

auto Scope::Canon(TypeSymbol &sym) const -> TypeSymbol* {
  // Only a generic parameter or generic argument can be
  // canonical-ised. For the generic parameter case, find
  // the binding by the generic parameter's parameter
  // identifier.
  if (sym.Kind == TypeKind::GenericParam and sym.ParamId != 0) {
    return FindParamBinding(
      *this, sym, sym.ParamId,
      [](Scope const &scope, auto const *name) { return scope.InternalTable.TypeTbl.Get(name); },
      [](Scope const &scope, auto const *name) { return SearchSupScopesForType(scope, name); });
  }

  // For the generic argument case, handle the bound parameter
  // identifier, ie the target of the generic argument.
  if (sym.Kind == TypeKind::GenericArg and sym.BindsParamId != 0) {
    return FindParamBinding(
      *this, sym, sym.BindsParamId,
      [](Scope const &scope, auto const *name) { return scope.InternalTable.TypeTbl.Get(name); },
      [](Scope const &scope, auto const *name) { return SearchSupScopesForType(scope, name); });
  }

  // A closed class names the same type from anywhere; there are
  // no generics that can be canonical-ised.
  if (sym.Kind == TypeKind::Class and sym.IsConcrete and sym.Alias == nullptr) { return &sym; }

  // Next we handle "Class" types that are not concrete. These are
  // "open" because not all the generics have been resolved. Check
  // the "instance of", ie the template, exists.
  if (sym.InstanceOf != nullptr and (sym.Kind == TypeKind::Class or sym.Alias != nullptr)) {
    // An instantiation whose recorded name names itself like
    // "FunMov[Args=Tup[FunMov[..], Args], ..]", with its inner
    // "FunMov" stamped with the outer one, it would re-key
    // through itself forever; while it is being re-keyed it
    // stands for itself.
    thread_local auto rekeying = std::vector<TypeSymbol const*>();
    if (genex::find(rekeying, &sym) != rekeying.end()) { return &sym; }
    rekeying.push_back(&sym);
    struct RekeyGuard {
      ~RekeyGuard() { rekeying.pop_back(); }
    } const _rekey_guard;

    // Get the generic argument group's identity key. If it
    // matches the currently inspected type symbol's identity key,
    // then return the symbol.
    const auto key = InstanceIdentityKey(sym.Name->GnArgGroup->GetAllArgs());
    if (key == sym.IdentityKey) { return &sym; }

    // Check the template's instantiations for this key; if it not
    // there, then invoke the hook to build the new instantiation.
    const auto hit = sym.InstanceOf->Instances.find(key);
    if (hit != sym.InstanceOf->Instances.end()) { return hit->second; }
    return OnInstantiationMissing ? OnInstantiationMissing(sym, *this) : nullptr;
  }

  // A template, or an alias, is its own declaration from anywhere.
  // Only a template stamp - the stripped head of a name written
  // with arguments - brings one here.
  if (sym.InstanceOf == nullptr and (sym.Kind == TypeKind::Class or sym.Alias != nullptr)) { return &sym; }
  return nullptr;
}

auto Scope::InstanceIdentityKey(
  Vec<GenericArgumentAst*> const &args, GenericParameterGroupAst const *params) const
  -> InstanceKey {
  using utils::cmp_utils::CompExprIdentity;
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

  // A type argument is what it resolves to: a parameter by its
  // identity, a binding by what it is bound to, an alias by its
  // target, anything else by its symbol. A convention is part of
  // the type, so it is part of the identity.
  const auto push_type = [this, &key, &push_spelling](TypeAst const &type) {
    // Push the convention tag with the convention's tag (could
    // be "&" or "&mut".
    if (const auto conv = type.GetConvention(); conv != nullptr) {
      key.Push(Tag::Conv, static_cast<std::uint64_t>(conv->Tag()));
    }

    // "Self" is keyed by its spelling (below), before it is read
    // as anything.
    if (type.IsSelfType()) {
      key.Push(Tag::Self);
      return;
    }

    // For a nullptr symbol, mark the identity for it as "unresolved".
    auto const *sym = GetTypeSymbol(&type);
    if (sym == nullptr) {
      push_spelling(Tag::Unresolved, type);
      return;
    }

    // Bind the alias into the symbol if it exists, and continue;
    // the rest of the function now operates on the alias type.
    if (sym->Alias != nullptr and sym->Alias->Resolved != nullptr) {
      if (const auto resolved = GetTypeSymbol(sym->Alias->Resolved.get()); resolved != nullptr) { sym = resolved; }
    }

    // For a generic argument, get the bound symbol, and if it
    // is the current symbol ie bound to this symbol, then get
    // the true generic value out of it and push that.
    if (sym->Kind == TypeKind::GenericArg) {
      const auto bound = sym->AsBoundSymbol();
      if (bound == sym) {
        key.Push(Tag::Bound, static_cast<std::uint64_t>(sym->BindsParamId));
        if (sym->GenericVal != nullptr) { push_spelling(Tag::Bound, *sym->GenericVal); }
        else { key.PushText(Tag::Bound, sym->Name->ToView()); }
        return;
      }
      sym = bound;
    }

    // For a generic parameter, just push the tag and the parameter's
    // interned identifier.
    if (sym->Kind == TypeKind::GenericParam) {
      key.Push(Tag::Param, static_cast<std::uint64_t>(sym->ParamId));
      return;
    }

    // "Self" is keyed by its spelling. Its meaning is per scope,
    // and an argument left as "Self" (a signature keeps it, to
    // be decided per call) is carried into the instance's own
    // sup blocks, where "Self" is a new instance at each level:
    // keyed by meaning, "Vec[Self]" minted "View[T=Self]",
    // "IterRef[T=Self]", etc..
    if (sym->Kind == TypeKind::Self) {
      key.Push(Tag::Self);
      return;
    }

    key.PushPtr(sym);
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
      push_type(*arg->TypeVal);
    }
    else if (arg->CompVal != nullptr) {
      comp_identity.clear();
      CompExprIdentity(*arg->CompVal, *this, comp_identity);
      key.PushText(Tag::Comp, comp_identity);
    }
  }
  return key;
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
  if (_SupScopesGen == generation and not OnSupScopesRead) {
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
  // A scope moving in the tree changes the method set and what
  // a lookup finds, so this is the coarsest of the three.
  ++_ScopeLinkageGeneration;
  ++_TypeStructureGeneration;
  ++_TypeLookupGeneration;
}

auto spp::analyse::scopes::TypeStructureGeneration()
  -> std::uint64_t {
  return _TypeStructureGeneration;
}

auto spp::analyse::scopes::BumpTypeStructureGeneration()
  -> void {
  ++_TypeStructureGeneration;
  ++_TypeLookupGeneration;
}

auto spp::analyse::scopes::TypeLookupGeneration()
  -> std::uint64_t {
  return _TypeLookupGeneration;
}
