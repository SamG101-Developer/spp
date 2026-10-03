module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbol_table;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.class_prototype_ast;
import spp.asts.closure_expression_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
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
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.compiler.module_tree;
import spp.lex.tokens;
import spp.utils.algorithms;
import spp.utils.error_formatter;
import spp.utils.interner;
import spp.utils.ptr;
import genex;

namespace spp::analyse::scopes {
  namespace {
    /// The fully qualified name that a super scope contributes
    /// as a super type, handling generics too, using a class
    /// symbol vs scope's LinkedTypeSymbol.
    auto SupTypeNameOf(Scope const *scope) -> Shared<TypeAst> {
      // The scope's own type symbol names it: an instantiation
      // with its arguments, a class or template as written.
      if (scope->LinkedTypeSymbol != nullptr) { return scope->LinkedTypeSymbol->FqName(); }
      const auto cls_proto = AstAs<ClassPrototypeAst>(scope->AstNode);
      const auto cls_sym = cls_proto != nullptr ? cls_proto->GetClsSymbol() : nullptr;
      return cls_sym != nullptr ? cls_sym->FqName() : nullptr;
    }

    /// The symbol (a variable or a type) a scope's super scopes file under "name": each direct super scope asked
    /// exclusively, which searches its own in turn, so the search stays in the sup graph rather than escaping into the
    /// enclosing lexical scopes.
    template <typename Symbol, typename Name>
    auto FindInSupScopesByName(Scope const &scope, Name const *name) -> Symbol* {
      for (const auto sup_scope : scope.GetDirectSupScopes()) {
        auto *const sym = [&] {
          if constexpr (std::same_as<Symbol, VariableSymbol>) { return sup_scope->FindVarSymbol(name, true); }
          else { return sup_scope->FindTypeSymbol(name, true); }
        }();
        if (sym != nullptr) { return sym; }
      }
      return nullptr;
    }

    /// The symbol "scope"'s super scopes file for a parameter identity, searched as a name is
    /// ("FindInSupScopesByName"): each direct super scope, then its own.
    template <typename Symbol, typename ByParam>
    auto FindInSupScopesByParam(Scope const &scope, const std::uint64_t id, ByParam const &by_param) -> Symbol* {
      for (const auto sup_scope : scope.GetSupScopes()) {
        if (auto *const found = by_param(*sup_scope, id); found != nullptr) { return found; }
      }
      return nullptr;
    }

    /// The binding a scope gives a generic parameter, by the parameter's identity
    /// ("IndividualSymbolTable::FindByParam"): the nearest scope filing one, where a scope asks its own table, then
    /// (inside a "sup" block, whose class's parameters are bound by the block's "Self" instantiation) that
    /// instantiation's; then the super scopes. A scope that files only the parameter itself (or a copy of it) leaves it
    /// unbound there, and the search goes on for a binding further out. Nothing binding it leaves the parameter itself.
    template <typename Symbol, typename ByParam>
    auto BindingOf(Scope const &from, Symbol &param, const std::uint64_t id, ByParam const &by_param) -> Symbol* {
      const auto binding = [id](Symbol *found) -> Symbol* {
        return found != nullptr and found->BindsParamId == id ? found : nullptr;
      };
      for (auto scope = &from; scope != nullptr; scope = scope->Parent) {
        if (auto *const found = binding(by_param(*scope, id)); found != nullptr) { return found; }
        if (const auto self_sym = scope->InternalTable.TypeTable.Find(
          spp::asts::generate::common_types_precompiled::SELF_TYPE->ToUnchecked<TypeIdentifierAst>());
          self_sym != nullptr and self_sym->IsSelf() and self_sym->LinkedScope != nullptr
          and self_sym->LinkedScope != scope) {
          if (auto *const found = binding(by_param(*self_sym->LinkedScope, id)); found != nullptr) { return found; }
        }
      }
      if (auto *const found = binding(FindInSupScopesByParam<Symbol>(from, id, by_param)); found != nullptr) {
        return found;
      }
      return &param;
    }

    /// The copy of "alias" (declared in a generic block, which has a copy per instantiation of that block, its target
    /// read through the instantiation's bindings, "CreateGnSupScope") that "scope" reaches: the one it means.
    /// Copies describe the same statement, which is how one is told from another alias of that name - a "use" of it
    /// included, which shares its name node but is a statement of its own.
    auto AliasCopyIn(Scope const &scope, TypeSymbol &alias) -> TypeSymbol* {
      auto *const here = scope.FindHeadSymbol(*alias.Name);
      return here != nullptr and here->Alias != nullptr and here->Alias->Stmt == alias.Alias->Stmt ? here : &alias;
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
  LinkedTypeSymbol(nullptr),
  LinkedNamespaceSymbol(nullptr),
  NonGnScope(this),
  _ErrorFormatter(error_formatter) {
}

Scope::Scope(Scope const &other) :
  Name(other.Name),
  Parent(other.Parent),
  AstNode(other.AstNode),
  LinkedTypeSymbol(other.LinkedTypeSymbol),
  LinkedNamespaceSymbol(other.LinkedNamespaceSymbol),
  NonGnScope(other.NonGnScope),
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
    std::move(scope_name), nullptr, nullptr, mod.Formatter.get());

  // Inject the "_global" namespace symbol into this scope to make
  // lookups orthogonal.
  auto glob_ns_sym_name = MakeShared<IdentifierAst>(0uz, "_global");
  auto glob_ns_sym = MakeShared<NamespaceSymbol>(
    std::move(glob_ns_sym_name), glob_scope.get());
  glob_scope->LinkedNamespaceSymbol = std::move(glob_ns_sym);

  // Return the global scope.
  return glob_scope;
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

auto Scope::GetGns() const
  -> Vec<Unique<GenericArgumentAst>> {
  // Create the symbols list.
  const auto scopes = GetAncestors();
  auto syms = Vec<Unique<GenericArgumentAst>>();
  auto type_names = Vec<Shared<TypeIdentifierAst>>();
  auto comp_names = Vec<Shared<IdentifierAst>>();

  // Check each ancestor scope, accumulating generic type
  // and comp symbols.
  for (const auto scope : scopes) {
    auto all_type_syms = scope->GetAllTypeSymbols(true)
      | genex::views::filter([](auto const &sym) { return sym->IsGn(); })
      | genex::to<Vec>();

    auto all_var_syms = scope->GetAllVarSymbols(true)
      | genex::views::filter([](auto const &sym) { return sym->IsGn(); })
      | genex::to<Vec>();

    // Bindings only, of both kinds: an unbound parameter is no generic argument of this scope.
    for (auto const &t : all_type_syms) {
      if (t->Kind == TypeKind::GnTypeParam) { continue; }
      if (t->LinkedSymbol() == t and t->BoundTypeVal == nullptr) { continue; }
      if (genex::contains(type_names, *t->Name, genex::meta::deref)) { continue; }
      syms.EmplaceBack(GenericArgumentAst::FromSymbol(*t));
      type_names.EmplaceBack(t->Name);
    }

    for (auto const &v : all_var_syms) {
      if (genex::contains(comp_names, *v->Name, genex::meta::deref)) { continue; }
      if (v->Kind == VariableKind::GnCompParam or v->BoundCompVal() == nullptr) { continue; }
      syms.EmplaceBack(GenericArgumentAst::FromSymbol(*v));
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
  InternalTable.VarTable.Add(sym->Name.get(), sym);
}

auto Scope::AddVarSymbolCheckConflict(
  Shared<VariableSymbol> const &sym)
  -> void {
  // Cannot allow for duplicate comptime definitions.
  const auto existing_sym = FindVarSymbol(sym->Name.get(), false);
  if (existing_sym != nullptr) {
    const auto is_functional = existing_sym->Kind == VariableKind::FnMock;

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
  InternalTable.VarTable.Add(sym->Name.get(), sym);
}

auto Scope::AddTypeSymbol(
  Shared<TypeSymbol> const &sym)
  -> void {
  // Add a type symbol to the corresponding symbol table.
  InternalTable.TypeTable.Add(sym->Name.get(), sym);
}

auto Scope::AddTypeSymbolCheckConflict(
  Shared<TypeSymbol> const &sym)
  -> void {
  // Cannot allow for duplicate definitions.
  const auto existing_sym = FindTypeSymbol(sym->Name.get(), false);
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
  InternalTable.TypeTable.Add(sym->Name.get(), sym);
}

auto Scope::AddNsSymbol(
  Shared<NamespaceSymbol> const &sym) -> void {
  // Add a namespace symbol to the corresponding symbol table.
  InternalTable.NsTable.Add(sym->Name.get(), sym);
}

auto Scope::AddNsSymbolCheckConflict(
  Shared<NamespaceSymbol> const &sym) -> void {
  // A namespace may not be declared twice in one scope.
  if (const auto existing_sym = FindNsSymbol(sym->Name.get(), true); existing_sym != nullptr) {
    Raise<errors::SppIdentifierDuplicateError>(
      {this}, ERR_ARGS(*existing_sym->Name, *sym->Name, "namespace identifier"));
  }
  InternalTable.NsTable.Add(sym->Name.get(), sym);
}

auto Scope::RemVarSymbol(
  IdentifierAst const *sym_name) -> Shared<VariableSymbol> {
  // Remove a variable symbol from the corresponding symbol table.
  return InternalTable.VarTable.Rem(sym_name);
}

auto Scope::RemTypeSymbol(
  TypeIdentifierAst const *sym_name) -> Shared<TypeSymbol> {
  // Remove a type symbol from the corresponding symbol table.
  return InternalTable.TypeTable.Rem(sym_name);
}

auto Scope::RemNsSymbol(
  IdentifierAst const *sym_name) -> Shared<NamespaceSymbol> {
  // Remove a namespace symbol from the corresponding symbol table.
  return InternalTable.NsTable.Rem(sym_name);
}

auto Scope::GetAllVarSymbols(
  const bool exclusive,
  const bool sup_scope_search) const
  -> Vec<VariableSymbol*> {
  // Yield all symbols from the var symbol table.
  auto syms = InternalTable.VarTable.GetAll();

  // For non-exclusive searches where a parent is present,
  // yield from the parent scope.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->GetAllVarSymbols(exclusive, sup_scope_search));
  }

  // For super scope searches, yield from all direct super
  // scopes.
  if (sup_scope_search) {
    for (auto const *sup_scope : GetSupScopes()) {
      syms.AppendRange(sup_scope->GetAllVarSymbols(true, false));
    }
  }

  return syms;
}

auto Scope::GetAllTypeSymbols(
  const bool exclusive,
  const bool sup_scope_search) const
  -> Vec<TypeSymbol*> {
  // Yield all symbols from the type symbol table.
  auto syms = InternalTable.TypeTable.GetAll();

  // For non-exclusive searches where a parent is present,
  // yield from the parent scope.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->GetAllTypeSymbols(exclusive, sup_scope_search));
  }

  // For super scope searches, yield from all super scopes.
  // Each is asked non-recursively, so the walk has to be over
  // the transitive closure rather than the direct list, which
  // is what the variable version above does.
  if (sup_scope_search) {
    for (auto const *sup_scope : GetSupScopes()) {
      syms.AppendRange(sup_scope->GetAllTypeSymbols(true, false));
    }
  }

  return syms;
}

auto Scope::GetAllNsSymbols(
  const bool exclusive) const -> Vec<NamespaceSymbol*> {
  auto syms = InternalTable.NsTable.GetAll();

  // For non-exclusive searches where a parent is present,
  // yield from the parent scope.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->GetAllNsSymbols(exclusive));
  }
  return syms;
}

auto Scope::HasVarSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not. The
  // lookup is borrowed, so this costs no refcount traffic.
  return FindVarSymbol(sym_name, exclusive) != nullptr;
}

auto Scope::HasTypeSymbol(
  TypeAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not.
  return FindTypeSymbol(sym_name, exclusive) != nullptr;
}

auto Scope::HasNsSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not.
  return FindNsSymbol(sym_name, exclusive) != nullptr;
}

auto Scope::FindVarSymbol(
  IdentifierAst const *sym_name,
  const bool exclusive,
  const bool sup_scope_search) const
  -> VariableSymbol* {
  // Get the symbol from the symbol table if it exists.
  if (sym_name == nullptr) { return nullptr; }

  // A name recording the comp parameter it named where it was
  // written means that parameter from here too, rather than
  // whatever its spelling names in this scope.
  if (not exclusive and sym_name->WrittenCompParamId() != 0) {
    if (auto *const canon = FindWrittenVarSymbol(sym_name->WrittenCompParamId()); canon != nullptr) { return canon; }
  }

  const auto scope = this;
  auto sym = InternalTable.VarTable.Find(sym_name);

  // If the symbol doesn't exist, and this is a non-exclusive
  // search, check the parent scope.
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->FindVarSymbol(sym_name, exclusive, sup_scope_search);
  }

  // If the symbol still hasn't been found, check the super
  // scopes for it.
  if (sym == nullptr and sup_scope_search) {
    sym = FindInSupScopesByName<VariableSymbol>(*scope, sym_name);
  }

  // Check for a linked aliased variable symbol.
  if (sym != nullptr and sym->AliasSymbol != nullptr) {
    sym = sym->AliasSymbol.get();
  }

  // Return the found symbol, or nullptr.
  return sym;
}

namespace {
  using spp::Pair;
  using spp::Vec;
  using spp::analyse::scopes::Scope;
  using spp::analyse::scopes::TypeId;
  using spp::analyse::scopes::TypeSymbol;
  using spp::asts::IdentifierAst;
  using spp::asts::TypeAst;
  using spp::asts::TypeIdentifierAst;

  /// Given a scope and a fully qualified type, move through the namespace parts of the type into the next namespace
  /// scopes. Returns the innermost scope and the unqualified type (for that scope). Only reached
  /// ("Scope::FindTypeSymbol") for a type that is not a plain identifier, so there is always a namespace or nested-type
  /// part to shift through.
  auto ShiftForNamespacedType(
    Scope const &scope, TypeAst const &fq_type)
    -> Pair<Scope const*, TypeIdentifierAst const*> {
    // Get the namespace and type parts, to get the scopes. Use
    // the appending form, so a type of any depth only uses one
    // allocation per list rather than one per level of the chain.
    auto ns_parts = Vec<IdentifierAst const*>();
    auto type_parts = Vec<TypeIdentifierAst const*>();
    fq_type.NsPartsInto(ns_parts);
    fq_type.TypePartsInto(type_parts);
    // The namespace parts first ("FindNsScope"): a part naming no namespace names no type.
    auto shifted_scope = scope.FindNsScope(ns_parts);
    if (shifted_scope == nullptr) { return {nullptr, type_parts.Back()}; }

    // Iterate through the type parts (except the final one) next.
    for (auto const *type_part : type_parts | genex::views::drop_last(1)) {
      const auto sym = shifted_scope->FindTypeSymbol(type_part);
      if (sym == nullptr or sym->IsGn()) { break; }
      shifted_scope = sym->LinkedScope;
    }

    // Return the type scope, and the final type part.
    auto const *final = type_parts.Back();
    return {shifted_scope, final};
  }

  /// The binary operator a comp identity's operation spells ("+", "<<"), as the token a written one carries.
  auto CompOperatorToken(
    const spp::StrView op)
    -> std::optional<spp::lex::SppTokenType> {
    using spp::lex::SppTokenType;
    for (const auto tok : {
           SppTokenType::TK_ADD, SppTokenType::TK_SUB, SppTokenType::TK_MUL, SppTokenType::TK_DIV, SppTokenType::TK_REM,
           SppTokenType::TK_BIT_IOR, SppTokenType::TK_BIT_AND, SppTokenType::TK_BIT_XOR, SppTokenType::TK_BIT_SHL,
           SppTokenType::TK_BIT_SHR, SppTokenType::TK_EQ, SppTokenType::TK_NE, SppTokenType::TK_LT, SppTokenType::TK_LE,
           SppTokenType::TK_GT, SppTokenType::TK_GE}) {
      if (spp::lex::TokToString(tok) == op) { return tok; }
    }
    return std::nullopt;
  }

  /// Push "type" keyed by its spelling under "tag": the id the interner gives its text, so what is pushed is a view of
  /// text that already exists where it can be (only a "TypeIdentifierAst" holds its stringification).
  auto PushSpelling(
    spp::analyse::scopes::InstanceKey &key, const spp::analyse::scopes::InstanceKey::Tag tag, TypeAst const &type)
    -> void {
    if (type.IsTypeIdentifier()) { key.PushText(tag, type.ToUnchecked<TypeIdentifierAst>()->ToView()); }
    else { key.PushText(tag, type.ToString()); }
  }

  /// The template a name with arguments instantiates, found from "scope", and the identity its arguments give that
  /// instance there ("Scope::InstanceIdOf"). The identity is only made when the template has instances to match.
  auto NamedInstance(
    Scope const &scope, TypeIdentifierAst const &name)
    -> Pair<TypeSymbol*, TypeId> {
    auto *const tmpl = scope.FindHeadSymbol(name);
    if (tmpl == nullptr or tmpl->Instances.empty()) { return {tmpl, nullptr}; }
    return {tmpl, scope.InstanceIdOf(*tmpl, name.GnArgGroup->GetAllArgs(), tmpl->GnParams())};
  }
}

auto Scope::FindTypeSymbol(
  TypeAst const *sym_name, const bool exclusive,
  const bool sup_scope_search) const -> TypeSymbol* {
  // Nullptr guard allows uniform lookups with no pre-checks
  // in callers.
  if (sym_name == nullptr) { return nullptr; }

  // A type carrying the identity it resolved to where it was
  // written means that identity from here too, rather than
  // whatever its spelling happens to name in this scope.
  if (not exclusive) {
    if (const auto written = sym_name->WrittenTypeId(); written != nullptr) {
      if (const auto found = FindWrittenTypeSymbol(written); found != nullptr) {
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
    if (scope_ == nullptr) { return nullptr; }
    scope = scope_;
    sym_name_extracted = sym_name_extracted_;
  }

  // A wrapper ("&T") hands the lookup to its last part, which
  // may carry a written identity of its own.
  if (not exclusive and sym_name_extracted != sym_name) {
    if (const auto written = sym_name_extracted->WrittenTypeId(); written != nullptr) {
      if (const auto found = FindWrittenTypeSymbol(written); found != nullptr) {
        return found;
      }
    }
  }

  // An instantiation is found by its template and what its arguments resolve to from here, not by its spelling. Both
  // checks below ask for it, so it is made at most once ("NamedInstance").
  const auto has_args = not sym_name_extracted->GnArgGroup->Args.IsEmpty();
  auto named = std::optional<Pair<TypeSymbol*, TypeId>>();
  const auto named_instance = [&]() -> Pair<TypeSymbol*, TypeId> const& {
    if (not named.has_value()) { named = NamedInstance(*scope, *sym_name_extracted); }
    return *named;
  };
  if (not exclusive and has_args) {
    if (auto const &[tmpl, id] = named_instance(); id != nullptr) {
      if (const auto hit = tmpl->Instances.find(id); hit != tmpl->Instances.end()) { return hit->second; }
    }
  }

  // Get the symbol from the symbol table if it exists.
  auto sym = scope->InternalTable.TypeTable.Find(sym_name_extracted);

  // If the symbol doesn't exist, and this is a non-exclusive
  // search, check the parent scope.
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->FindTypeSymbol(sym_name_extracted, exclusive, sup_scope_search);
  }

  // If the symbol still hasn't been found, check the super
  // scopes for it.
  if (sym == nullptr and sup_scope_search) {
    sym = FindInSupScopesByName<TypeSymbol>(*scope, sym_name_extracted);
  }

  // An instantiation is answered by identity, never by spelling. The table holds one entry per spelling, so
  // "Mutex[T=T]" written in the class and in "sup [T: Drop] Mutex[T]" are one entry and two types: the second names the
  // block's own parameter, which carries its constraints. A hit that instantiates this name's template under another
  // identity is not the type being asked for.
  if (sym != nullptr and sym->InstanceOf != nullptr and has_args) {
    if (auto const &[tmpl, id] = named_instance(); tmpl == sym->InstanceOf and sym->Id != id) { sym = nullptr; }
  }

  // Return the found symbol, or nullptr.
  return sym;
}

auto Scope::FindSelfSymbol(
  const bool exclusive) const -> TypeSymbol* {
  using generate::common_types_precompiled::SELF_TYPE;
  return FindTypeSymbol(SELF_TYPE.get(), exclusive);
}

auto Scope::FindHeadSymbol(
  TypeAst const &type, const bool exclusive) const -> TypeSymbol* {
  return FindTypeSymbol(type.WithoutGns().get(), exclusive);
}

auto Scope::CanonVar(VariableSymbol &sym) const -> VariableSymbol* {
  // As "CanonType": a comp parameter by its identifier, and a binding by the parameter it binds.
  const auto by_param = [](Scope const &scope, const std::uint64_t id) {
    return scope.InternalTable.VarTable.FindByParam(id);
  };
  const auto param = sym.ParamId();
  return param != 0 ? BindingOf(*this, sym, param, by_param) : nullptr;
}

auto Scope::CanonType(TypeSymbol &sym) const -> TypeSymbol* {
  // A generic parameter, or a binding of one, is the binding of that parameter here, re-linked first if it was bound
  // to an alias whose target is made now ("Rebind").
  if (const auto param = sym.ParamId(); param != 0) {
    auto *const binding = BindingOf(*this, sym, param, [](Scope const &scope, const std::uint64_t id) {
      return scope.InternalTable.TypeTable.FindByParam(id);
    });
    if (binding != &sym) { binding->Rebind(); }
    return binding;
  }

  // A closed class names the same type from anywhere; there are
  // no generics that can be canonical-ised.
  if (sym.Kind == TypeKind::Cls and sym.Alias == nullptr and sym.IsConcrete) { return &sym; }

  // An open instantiation is its identity read through this scope's bindings ("ReadIn"): the instantiation filed under
  // that. One not made yet has no answer here: a lookup makes nothing ("TypeRef::Of" does).
  if (sym.InstanceOf != nullptr and (sym.Kind == TypeKind::Cls or sym.Alias != nullptr)) {
    return sym.Id != nullptr ? TypeSymbolOf(ReadIn(sym.Id)) : nullptr;
  }

  if (sym.InstanceOf == nullptr and sym.Alias != nullptr) { return AliasCopyIn(*this, sym); }

  // Anything else names itself from anywhere: a template (only its written identity, the stripped head of a name
  // written with arguments, brings one here), a mock, a class or alias with nothing to re-read.
  return &sym;
}

auto Scope::ArgsIdOf(
  Vec<GenericArgumentAst*> const &args, GenericParameterGroupAst const *params) const
  -> TypeId {
  using Tag = InstanceKey::Tag;

  // Start with an empty instance key. This will get built upon
  // as the generics are considered.
  auto key = InstanceKey();

  // Named by keyword where it can be, by position otherwise (a
  // tuple's arguments stay positional). A comp argument is its
  // value's identity: folded where closed, its parameters by
  // identity where not, so "n + 1" under two bindings of "n"
  // is two keys and "(n + 1)" is "n + 1".
  // A positional argument is keyed under the parameter it binds ("type_resolution::ParamsOfArgs"), as naming it would.
  const auto targets = params != nullptr
    ? utils::type_resolution::ParamsOfArgs(args, params->GetAllParams())
    : Vec<GenericParameterAst*>();
  for (auto i = 0uz; i < args.Len(); ++i) {
    const auto arg = args[i];
    auto const *const param = arg->TypeName() == nullptr and i < targets.Len() ? targets[i] : nullptr;

    // Push the argument name into the overall key. This works for
    // keyword arguments.
    if (arg->TypeName() != nullptr) {
      PushSpelling(key, Tag::Name, *arg->TypeName());
    }
    // If we have variadics, then the push the parameter's name,
    // which will bind the tuple of variadic arguments going in.
    else if (param != nullptr and not param->IsVariadic()) {
      PushSpelling(key, Tag::Name, *param->Name);
    }
    // Otherwise we have a genuinely positional argument, so push
    // the index.
    else {
      key.Push(Tag::Pos, i);
    }

    // Each argument is one part, by its identity: a type keyed once (by id, or whole when a part of it is
    // unresolved, "PushTypePart"), a comp value by its "CompId" ("PushCompPart"). Nothing is recorded: an identity
    // names its own value ("CompAstOf"), an opaque part's having been recorded as it was keyed ("comp_generics::CompKey").
    if (arg->IsTypeArg()) { PushTypePart(key, TypeKey(*arg->TypeVal)); }
    else if (arg->IsCompArg()) { PushCompPart(key, CompIdOf(*arg->CompVal)); }
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

  /// "alias" marked as being keyed through its target ("keying_aliases") for as long as this lives, unless it already
  /// is: only the outermost keying of an alias ("Active") may expand it.
  class AliasKeying {
  public:
    explicit AliasKeying(TypeSymbol const &alias) : _Active(not genex::contains(keying_aliases, &alias)) {
      if (_Active) { keying_aliases.push_back(&alias); }
    }

    ~AliasKeying() { if (_Active) { keying_aliases.pop_back(); } }

    AliasKeying(AliasKeying const &) = delete;
    auto operator=(AliasKeying const &) -> AliasKeying& = delete;

    SPP_ATTR_NODISCARD auto Active() const -> bool { return _Active; }

  private:
    bool _Active;
  };
}


namespace {
  /// The key of a written type ("t"), or of a type already resolved to "given" (a "TypeRef"'s symbol), read in "scope";
  /// and when it is a variant, the keys of its members, so a variant member of a variant is flattened into it.
  SPP_ATTR_HOT auto KeyIn(Scope const &scope, TypeAst const *t, TypeSymbol const *given) -> TypeKeyParts;

  /// Continue "out" with a key already made: a variant's members are carried with it, so a variant it is a member of
  /// flattens it.
  SPP_ATTR_HOT auto AppendKeyId(
    TypeKeyParts &out, const TypeId id) -> void {
    using namespace spp::analyse::scopes;
    out.Key.AppendKey(*id);
    auto const &head = HeadOf(id);
    if (head.Kind == InstanceKey::Tag::Variant and head.Conv == 0) {
      for (const auto m : head.Members) { out.Members.EmplaceBack(InstanceKey(*m)); }
    }
  }

  /// The alias an identity names whole, when it is one: a name stamped as an alias ("WrittenTypeIdOf") is keyed as the
  /// alias's target when it is read, as the alias itself would be.
  auto AliasHeadOf(
    const spp::analyse::scopes::TypeId id) -> spp::analyse::scopes::TypeSymbol const* {
    using spp::analyse::scopes::HeadOf;
    using spp::analyse::scopes::InstanceKey;
    if (id == nullptr or HeadOf(id).Kind != InstanceKey::Tag::Symbol or HeadOf(id).Conv != 0) { return nullptr; }
    auto const *const sym = HeadOf(id).Symbol();
    return sym != nullptr and sym->Alias != nullptr ? sym : nullptr;
  }

  /// An alias keyed as its target ("TypeSymbol::AliasTargetId"). One already being keyed through (a target naming its
  /// own alias) is keyed as itself; one with no target as "unresolved"'s spelling, where given, else as itself.
  auto KeyAliasTarget(
    TypeKeyParts &out, TypeSymbol const &alias, TypeAst const *unresolved) -> void {
    using spp::analyse::scopes::InstanceKey;
    const auto keying = AliasKeying(alias);
    if (not keying.Active()) {
      out.Key.PushPtr(&alias);
      return;
    }
    const auto target = alias.AliasTargetId();
    if (target == nullptr) {
      if (unresolved != nullptr) { PushSpelling(out.Key, InstanceKey::Tag::Unresolved, *unresolved); }
      else { out.Key.PushPtr(&alias); }
      return;
    }

    // A target recorded as another alias (stamped before that alias's own statement resolved it) is that alias's target.
    if (auto const *const next = AliasHeadOf(target); next != nullptr) {
      KeyAliasTarget(out, *next, nullptr);
      return;
    }
    AppendKeyId(out, target);
  }

  /// A name written as an open instantiation is its written identity read here ("ReadIn"); one written as an
  /// instantiation of an alias is the alias's target read here. An open instantiation of an alias names its arguments
  /// where it was written, which its own target is recorded in terms of: that target read here, else "open_alias" is
  /// set and its written arguments are read here instead ("KeyUnmade"). True when "t" is keyed.
  auto KeyWritten(
    TypeKeyParts &out, Scope const &scope, TypeAst const &t, bool &open_alias) -> bool {
    using namespace spp::analyse::scopes;
    using Tag = InstanceKey::Tag;
    const auto written = t.LastTypePart()->WrittenTypeId();
    if (written == nullptr) { return false; }
    if (auto const *const alias = AliasHeadOf(written); alias != nullptr) {
      KeyAliasTarget(out, *alias, nullptr);
      return true;
    }

    // Read here (an operation over a parameter bound here is rewritten and folded by identity,
    // "scopes::RewriteCompKey"); one whose identity does not read so is keyed from its written arguments.
    auto const *const named = HeadOf(written).Kind == Tag::Inst ? scope.TypeSymbolOf(written) : nullptr;
    const auto read = named != nullptr and named->Kind == TypeKind::Cls and named->Alias == nullptr
      and not named->IsConcrete ? scope.ReadIn(written) : nullptr;
    if (read != nullptr) {
      AppendKeyId(out, read);
      return true;
    }
    if (named == nullptr or named->Alias == nullptr) { return false; }
    open_alias = DoesTypeIdNameParams(written);
    if (not open_alias) {
      KeyAliasTarget(out, *named, nullptr);
      return true;
    }

    // An open instantiation of an alias stands for its target ("TypeSymbol::AliasTargetId"), recorded in the same
    // terms as its arguments: that target read here.
    if (const auto target = named->AliasTargetId(); target != nullptr and HeadOf(target).IsInstance()) {
      if (const auto target_read = scope.ReadIn(target); target_read != nullptr) {
        AppendKeyId(out, target_read);
        return true;
      }
    }
    return false;
  }

  /// A name nothing resolves (an instantiation not made, reached by spelling) is keyed by what it names: its arguments
  /// recorded as the instantiation would record them ("RecordedArgsFor"), then an alias as its target with them
  /// substituted, a class as its template and them. True when "t" is keyed.
  auto KeyUnmade(
    TypeKeyParts &out, Scope const &scope, TypeAst const &t) -> bool {
    auto const &written_args = *t.LastTypePart()->GnArgGroup;
    auto const *head = scope.FindHeadSymbol(t);

    // A "use" passes its arguments straight to what it names, so it is followed to that first: a class, or a "type"
    // alias ("use ..::SizedIntegerSigned" names the alias "SizedIntegerSigned[cmp w]").
    auto const *const named = head != nullptr ? head->UseTarget() : nullptr;
    if (named != nullptr and named->Alias != nullptr and not named->Alias->IsFromUseStmt) {
      // A "type" alias: its target ("TypeSymbol::AliasTargetId"), its parameters bound to the arguments.
      const auto keying = AliasKeying(*named);
      auto const &alias = *named->Alias;
      if (keying.Active() and alias.Params != nullptr and alias.WrittenIn != nullptr) {
        const auto recorded = spp::analyse::utils::type_resolution::RecordedArgsFor(
          written_args, *alias.Params, scope, *alias.WrittenIn);
        const auto recorded_args = recorded | genex::views::ptr | genex::to<spp::Vec>();
        if (not recorded.IsEmpty()) {
          if (const auto id = named->AliasTargetId(scope.ArgsIdOf(recorded_args)); id != nullptr) {
            AppendKeyId(out, id);
            return true;
          }
        }
      }
    }

    // A class, as its template and its arguments. A variadic class (a variant, a tuple) has no defaults to record:
    // its arguments are keyed as written.
    if (named != nullptr and named != head and named->Alias == nullptr) { head = named; }
    if (head == nullptr or head->Alias != nullptr or head->Type == nullptr or head->InstanceOf != nullptr) {
      return false;
    }
    if (head->Type->GnParamGroup->GetVariadicParam() != nullptr) {
      AppendKeyId(out, scope.InstanceIdOf(*head, written_args.GetAllArgs()));
      return true;
    }
    const auto recorded = spp::analyse::utils::type_resolution::RecordedArgsFor(
      written_args, *head->Type->GnParamGroup, scope, *head->LinkedScope);
    if (recorded.IsEmpty()) { return false; }
    AppendKeyId(out, scope.InstanceIdOf(*head, recorded | genex::views::ptr | genex::to<spp::Vec>()));
    return true;
  }

  /// A binding ("T" bound to "Str", written "t" where it was written), keyed as it holds its value. What it is bound
  /// to, for "KeySymbol" to key; null when the binding is keyed whole here.
  auto KeyBinding(
    TypeKeyParts &out, Scope const &scope, TypeSymbol const &sym, TypeAst const *t) -> TypeSymbol const* {
    using namespace spp::analyse::scopes;
    using spp::asts::ConventionTag;
    using Tag = InstanceKey::Tag;

    // Held as the binding holds it ("T" bound to "&mut Str" is "&mut Str"), as "TypeRef::Of" reads it, unless written
    // with a convention of its own.
    const auto held = sym.HeldConvention();
    if (held != ConventionTag::MOV and (t == nullptr or t->GetConvention() == nullptr)) {
      out.Key.Push(Tag::Conv, static_cast<std::uint64_t>(held));
    }

    // Bound to an alias whose target is not made yet ("U8", before its "SizedInteger" instance is), it links the
    // target's template; the alias keys as its target does, by the arguments it records, with nothing made.
    sym.Rebind();
    if (sym.BoundAlias != nullptr) {
      auto inner = KeyIn(scope, nullptr, sym.BoundAlias);
      out.Key.AppendKey(inner.Key);
      out.Members = std::move(inner.Members);
      return nullptr;
    }
    auto const *const bound = sym.AsBound();

    // Nothing is bound to a bare generic template: linking one means the instance its value names is not made yet.
    // The value's recorded identity, where closed, is that instance.
    if (bound != &sym and bound->IsBareTemplate() and sym.BoundTypeVal != nullptr) {
      if (const auto written = sym.BoundTypeVal->LastTypePart()->WrittenTypeId();
        IsClosedTypeId(written) and HeadOf(written).IsInstance()) {
        AppendKeyId(out, written);
        return nullptr;
      }
    }
    if (bound == &sym) {
      out.Key.Push(Tag::TypeBound, static_cast<std::uint64_t>(sym.BindsParamId));
      if (sym.BoundTypeVal != nullptr) { PushSpelling(out.Key, Tag::TypeBound, *sym.BoundTypeVal); }
      else { out.Key.PushText(Tag::TypeBound, sym.Name->ToView()); }
      return nullptr;
    }
    return bound;
  }

  /// A type resolved to "sym" (written "t", where it was written): an alias as its target, a binding as what it is
  /// bound to, a parameter by its identity, "Self" by spelling, a made instantiation as the identity it was filed under
  /// ("Scope::InstanceIdOf"), anything else by its address.
  auto KeySymbol(
    TypeKeyParts &out, Scope const &scope, TypeSymbol const &sym, TypeAst const *t) -> void {
    using namespace spp::analyse::scopes;
    using Tag = InstanceKey::Tag;
    if (sym.Alias != nullptr and sym.Alias->Resolved != nullptr) {
      KeyAliasTarget(out, sym, sym.Alias->Resolved.get());
      return;
    }
    auto const *keyed = &sym;
    if (keyed->Kind == TypeKind::GnTypeArg) {
      keyed = KeyBinding(out, scope, *keyed, t);
      if (keyed == nullptr) { return; }
    }
    if (keyed->Kind == TypeKind::GnTypeParam) {
      out.Key.Push(Tag::TypeParam, static_cast<std::uint64_t>(keyed->OwnParamId));
    }
    else if (keyed->Kind == TypeKind::Self) { out.Key.Push(Tag::Self); }
    else if (keyed->Kind == TypeKind::Cls and keyed->InstanceOf != nullptr) { AppendKeyId(out, keyed->Id); }
    else { out.Key.PushPtr(keyed); }
  }

  SPP_ATTR_HOT auto KeyIn(
    Scope const &scope, TypeAst const *t, TypeSymbol const *given) -> TypeKeyParts {
    using namespace spp::analyse::scopes;
    using Tag = InstanceKey::Tag;
    auto out = TypeKeyParts();

    // A convention is part of the type. "Self" is keyed by its spelling, before it is read as anything: its meaning
    // is per scope, and an argument left as "Self" is carried into an instance's own sup blocks, where it is a new
    // instance at each level - keyed by meaning, "Vec[Self]" minted "View[T=Self]", "IterRef[T=Self]", etc.
    if (t != nullptr) {
      if (const auto conv = t->GetConvention(); conv != nullptr) {
        out.Key.Push(Tag::Conv, static_cast<std::uint64_t>(conv->Tag()));
      }
      if (t->IsSelfType()) {
        out.Key.Push(Tag::Self);
        return out;
      }
    }
    auto open_alias = false;
    if (t != nullptr and KeyWritten(out, scope, *t, open_alias)) { return out; }

    // Anything else is what it resolves to here. A name recording a parameter is that parameter's binding here, keyed
    // as a binding; the lookup answers what the binding links, which for one still waiting on an alias's target is only
    // that target's template.
    auto const *sym = given;
    if (sym == nullptr and t != nullptr and not open_alias) {
      if (const auto written = t->LastTypePart()->WrittenTypeId();
        written != nullptr and HeadOf(written).Kind == Tag::TypeParam) { sym = scope.FindWrittenTypeSymbol(written); }
      if (sym == nullptr) { sym = scope.FindTypeSymbol(t); }
    }
    if (sym == nullptr and t != nullptr and not t->LastTypePart()->GnArgGroup->Args.IsEmpty()
      and KeyUnmade(out, scope, *t)) { return out; }
    if (sym == nullptr) {
      PushSpelling(out.Key, Tag::Unresolved, *t);
      return out;
    }
    KeySymbol(out, scope, *sym, t);
    return out;
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
      auto parts = KeyIn(read_in, member, nullptr);
      auto flat = parts.Members.IsEmpty() ? spp::Vec<InstanceKey>{std::move(parts.Key)} : std::move(parts.Members);
      for (auto &m : flat) {
        if (not genex::contains(out.Members, m)) { out.Members.EmplaceBack(std::move(m)); }
      }
    }
    out.Members |= genex::actions::sort([](auto const &a, auto const &b) { return a < b; });
    out.Key.Push(InstanceKey::Tag::Variant);
    out.Key.PushPtr(&tmpl);
    for (auto const &m : out.Members) { PushTypePart(out.Key, InstanceKey(m)); }
    return out;
  }
}

auto Scope::InstanceIdOf(
  TypeSymbol const &tmpl, Vec<GenericArgumentAst*> const &args, GenericParameterGroupAst const *params) const
  -> TypeId {
  // A variant is the set of its members; anything else its template and its arguments - an alias of a variant too,
  // whose own arguments are not its members ("Opt[T]").
  // The template is compared as it is, its identity being what is built here.
  using asts::generate::common_types_precompiled::VAR;
  auto const *const tmpl_of = tmpl.InstanceOf != nullptr ? tmpl.InstanceOf : &tmpl;
  if (tmpl.Alias == nullptr and tmpl_of == PrecompiledTemplate(*VAR, *this)) {
    // The members: the tuple named "Variants" (the analysed form), a lone tuple, or - written "A or B" before it is
    // analysed - the positional arguments themselves.
    auto const *variants = static_cast<GenericArgumentAst const*>(nullptr);
    for (auto const *arg : args) { if (arg->TypeName() != nullptr and arg->ViewName() == "Variants") { variants = arg; } }
    if (variants == nullptr and args.Len() == 1 and args[0]->TypeName() == nullptr) { variants = args[0]; }
    auto members = Vec<TypeAst const*>();
    if (variants != nullptr and variants->IsTypeArg()) {
      for (auto const *member : variants->TypeVal->LastTypePart()->GnArgGroup->GetTypeArgs()) { members.EmplaceBack(member->TypeVal.get()); }
    }
    else {
      for (auto const *arg : args) { if (arg->TypeName() == nullptr and arg->IsTypeArg()) { members.EmplaceBack(arg->TypeVal.get()); } }
    }
    return InternTypeKey(VariantKey(tmpl, members, *this).Key);
  }
  return InstanceIdOfArgs(tmpl, ArgsIdOf(args, params));
}

auto Scope::TypeKey(
  TypeAst const &type) const
  -> InstanceKey {
  return KeyIn(*this, &type, nullptr).Key;
}

auto Scope::TypeIdOfSymbol(
  TypeSymbol const &sym,
  const std::uint64_t conv) const
  -> TypeId {
  auto key = InstanceKey();
  if (conv != 0) { key.Push(InstanceKey::Tag::Conv, conv); }
  key.AppendKey(KeyIn(*this, nullptr, &sym).Key);
  return key.HasUnresolved ? nullptr : InternTypeKey(std::move(key));
}


// D7 ("docs/analyse-cleanup-review.md"): what reads an identity here then looks it up ("FindWrittenTypeSymbol",
// "CanonType"'s instance case) becomes "TypeRef::ReadIn(scope).Symbol"; this stays the raw, no-make lookup under it.
auto Scope::TypeSymbolOf(
  const TypeId id) const
  -> TypeSymbol* {
  using Tag = InstanceKey::Tag;
  if (id == nullptr or id->HasUnresolved) { return nullptr; }

  auto const &head = HeadOf(id);
  auto *const ptr = head.Symbol();
  switch (head.Kind) {
  case Tag::Symbol:
    return ptr;
  case Tag::TypeParam:
    return GnTypeParamOf(head.TypeParamId);
  case Tag::Inst:
  case Tag::Variant: {
    const auto hit = ptr->Instances.find(BareTypeId(id));
    return hit != ptr->Instances.end() ? hit->second : nullptr;
  }
  case Tag::Self: {
    auto *const self = FindSelfSymbol();
    return self != nullptr ? self->AsBound() : nullptr;
  }
  default:
    return nullptr;
  }
}

auto Scope::ReadIn(
  const TypeId written) const
  -> TypeId {
  if (written == nullptr) { return nullptr; }
  if (not DoesTypeIdNameParams(written)) { return written; }
  auto const &params = ParamsOf(written);

  auto subst = GenericSubst();
  for (const auto pid : params.TypeParams) {
    auto *const param = GnTypeParamOf(pid);
    auto *const binding = param != nullptr ? CanonType(*param) : nullptr;
    if (binding == nullptr or binding == param) { continue; }
    // Held as the binding holds it ("Splice" keeps the parameter's own convention where it is written with one).
    const auto bound = TypeIdOfSymbol(*binding, 0);
    if (bound == nullptr) { continue; }
    auto const &head = HeadOf(bound);
    const auto is_itself = (head.Kind == InstanceKey::Tag::TypeParam or head.Kind == InstanceKey::Tag::TypeBound)
      and head.TypeParamId == pid;
    if (is_itself) { continue; }
    subst.TypeParams.emplace_back(pid, bound);
    if (param->IsVariadic) { subst.TypePackParams.push_back(pid); }
  }
  for (const auto comp : params.CompParams) {
    auto *const param = GnCompParamOf(comp);
    auto *const binding = param != nullptr ? CanonVar(*param) : nullptr;
    if (binding == nullptr or binding == param) { continue; }
    // Keyed from the binding's symbol, as a type binding is ("TypeIdOfSymbol"), not its name spelled again here, where
    // a nearer symbol of that name can answer instead.
    const auto bound = CompIdOfSymbol(*binding);
    if (bound == ParamCompId(comp)) { continue; }
    subst.CompParams.emplace_back(comp, bound);
    if (param->IsVariadic) { subst.CompPackParams.push_back(comp); }
  }
  return SubstituteTypeId(written, subst);
}

auto Scope::VarSymbolOf(
  const std::uint64_t comp_param) const
  -> VariableSymbol* {
  return GnCompParamOf(comp_param);
}

auto Scope::FindWrittenTypeSymbol(
  const TypeId written) const
  -> TypeSymbol* {
  // As "FindWrittenVarSymbol": the symbol the identity names, as this scope means it ("CanonType") - a parameter's
  // binding, an instance read through these bindings, anything else itself. "Self" is keyed by its spelling, so is
  // looked up by name instead ("TypeSymbolOf" would answer the class it stands for, not the "Self" symbol).
  if (written == nullptr or HeadOf(written).Kind == InstanceKey::Tag::Self) { return nullptr; }
  auto *const sym = TypeSymbolOf(written);
  return sym != nullptr ? CanonType(*sym) : nullptr;
}

auto Scope::FindWrittenVarSymbol(
  const std::uint64_t comp_param) const
  -> VariableSymbol* {
  auto *const param = VarSymbolOf(comp_param);
  return param != nullptr ? CanonVar(*param) : nullptr;
}


auto Scope::CompIdOf(
  ExpressionAst const &value) const
  -> CompId {
  return InternCompKey(utils::comp_generics::CompKey(value, *this));
}

auto Scope::CompIdOfSymbol(
  VariableSymbol const &sym) const
  -> CompId {
  if (auto const *const bound = sym.BoundCompVal(); bound != nullptr) { return CompIdOf(*bound); }
  return ParamCompId(sym.ParamId());
}


auto Scope::CompAstOf(
  const CompId id) const
  -> Shared<ExpressionAst> {
  auto const *const node = CompNodeOf(id);
  return node != nullptr ? Shared<ExpressionAst>(CompAstOf(*node)) : nullptr;
}

auto Scope::CompAstOf(
  CompNode const &node) const
  -> Unique<ExpressionAst> {
  using Kind = CompNode::Part;
  switch (node.Kind) {
  case Kind::Value:
    return utils::comp_generics::CompValueAst(node.Text);
  case Kind::Param: {
    auto const *const param = GnCompParamOf(node.ParamId);
    if (param == nullptr) { return nullptr; }
    auto name = AstClone(param->Name.get());
    name->SetWrittenCompParamId(node.ParamId);
    return name;
  }
  case Kind::Pack: {
    auto values = Vec<Unique<ExpressionAst>>();
    for (auto const &elem : node.Kids) {
      auto value = CompAstOf(elem);
      if (value == nullptr) { return nullptr; }
      values.EmplaceBack(std::move(value));
    }
    return MakeUnique<TupleLiteralAst>(nullptr, std::move(values), nullptr);
  }
  case Kind::Op: {
    const auto tok = CompOperatorToken(node.Text);
    auto lhs = CompAstOf(node.Kids[0]);
    auto rhs = CompAstOf(node.Kids[1]);
    if (not tok.has_value() or lhs == nullptr or rhs == nullptr) { return nullptr; }
    return MakeUnique<BinaryExpressionAst>(
      std::move(lhs), MakeUnique<TokenAst>(0uz, *tok, lex::TokToString(*tok)), std::move(rhs));
  }
  case Kind::Opaque: {
    auto const *const recorded = utils::comp_generics::OpaqueCompValue(InternCompKey(node));
    return recorded != nullptr ? AstClone(recorded) : nullptr;
  }
  case Kind::Member: {
    auto owner = TypeAstOf(TypeIdOfWord(node.Type));
    if (owner == nullptr) { return nullptr; }
    return MakeUnique<PostfixExpressionAst>(
      AstClone(owner.get()), MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
        nullptr, MakeShared<IdentifierAst>(0uz, node.Text)));
  }
  default:
    return nullptr;
  }
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
  auto *const sym = bare->HasSelf or head.IsInstance() ? nullptr : TypeSymbolOf(bare);
  if (sym != nullptr) { out = sym->FqName(); }
  else {
    switch (head.Kind) {
    case Tag::TypeParam:
    case Tag::TypeBound: {
      // Stamped, so it is that parameter wherever it is read, not whatever shares its spelling there. A binding with no
      // type of its own to name is its parameter too, read through the binding wherever that is in view.
      auto const *const param = GnTypeParamOf(head.TypeParamId);
      if (param == nullptr) { return nullptr; }
      out = AstCloneShared(param->FqName().get());
      out->LastTypePart()->SetWrittenTypeId(head.Kind == Tag::TypeParam ? bare : WrittenTypeIdOf(*param));
      break;
    }
    case Tag::Self:
      out = AstCloneShared(SELF_TYPE.get());
      break;
    case Tag::Inst: {
      auto const *const tmpl = head.Symbol();
      auto group = GenericArgumentGroupAst::NewEmpty();
      for (auto const &arg : ArgsOf(head.Args)) {
        auto name = arg.Named
          ? TypeIdentifierAst::FromString(Str(WordText(arg.Name)))
          : nullptr;
        if (arg.TypeVal != nullptr) {
          auto type = TypeAstOf(arg.TypeVal);
          if (type == nullptr) { return nullptr; }
          group->Args.EmplaceBack(GenericArgumentAst::NewType(std::move(name), std::move(type)));
          continue;
        }
        auto value = CompAstOf(arg.CompVal);
        if (value == nullptr) { return nullptr; }
        group->Args.EmplaceBack(GenericArgumentAst::NewComp(std::move(name), AstClone(value.get())));
      }
      // Headed by the template it instantiates, recorded so the head resolves to it from anywhere. An alias by its own
      // name: its qualified name is its target's ("Var[..]"), which the alias's arguments do not name.
      auto head = tmpl->Alias != nullptr ? AstCloneShared(tmpl->Name.get()) : tmpl->FqName()->WithoutGns();
      out = head->WithGns(std::move(group));
      out->LastTypePart()->SetWrittenTemplateId(WrittenTypeIdOf(*tmpl));
      if (IsStampableTypeId(bare)) { out->LastTypePart()->SetWrittenTypeId(bare); }
      break;
    }
    case Tag::Variant: {
      // In the order an analysed variant lists them ("type_compare::OrderVariantMembers"), which a pattern over it is
      // matched in.
      auto unordered = Vec<Pair<Shared<TypeAst>, TypeSymbol const*>>();
      for (const auto member : head.Members) {
        auto type = TypeAstOf(member);
        if (type == nullptr) { return nullptr; }
        unordered.EmplaceBack(std::move(type), TypeSymbolOf(BareTypeId(member)));
      }
      auto members = utils::type_compare::OrderVariantMembers(std::move(unordered));
      // In the analysed form, which holds the members as one tuple ("Var[Variants=Tup[..]]").
      out = generate::common_types::VariantType(0, {});
      out->LastTypePart()->GnArgGroup->Args.EmplaceBack(GenericArgumentAst::NewType(
        TypeIdentifierAst::FromString("Variants"), generate::common_types::TupleType(0, std::move(members))));
      if (IsStampableTypeId(bare)) {
        out->LastTypePart()->SetWrittenTypeId(bare);
        // The tuple of members is a type in its own right, keyed from its (recorded) members, so it too means the same
        // wherever it is read.
        if (auto const *const variants = out->LastTypePart()->GnArgGroup->At("Variants"); variants != nullptr) {
          if (const auto tup_id = TypeIdOf(*variants->TypeVal); tup_id != nullptr) {
            variants->TypeVal->LastTypePart()->SetWrittenTypeId(tup_id);
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

auto Scope::PartialTypeIdOf(
  TypeAst const &type) const
  -> TypeId {
  return InternTypeKey(TypeKey(type));
}

auto Scope::FindNsSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const
  -> NamespaceSymbol* {
  // Get the symbol from the symbol table if it exists.
  if (sym_name == nullptr) { return nullptr; }
  const auto scope = this;
  auto sym = InternalTable.NsTable.Find(sym_name);

  // If the symbol doesn't exist, and this is a non-exclusive
  // search, check the parent scope.
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->FindNsSymbol(sym_name, exclusive);
  }

  // Return the found symbol, or nullptr.
  return sym;
}

auto Scope::FindVarSymbolOutermost(
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
    auto sym = FindVarSymbol(adjusted_name->To<IdentifierAst>());
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
      const auto type_sym = FindTypeSymbol(type_lhs);
      if (type_sym == nullptr or type_sym->LinkedScope == nullptr) { return {nullptr, this}; }
      const auto var_sym = type_sym->LinkedScope->FindVarSymbol(postfix_op->Name.get());
      return {var_sym, const_cast<Scope const*>(type_sym->LinkedScope)};
    }

    // Namespace based left-hand-side, such as
    // "a::b::c::my_function()"
    auto namespace_scope = this;
    if (is_valid_postfix_expression_static(adjusted_name)) {
      adjusted_name = adjusted_name->To<PostfixExpressionAst>()->Lhs.get();
      namespace_scope = namespace_scope->ConvertPostfixToNestedScope(adjusted_name->To<ExpressionAst>());
    }
    auto sym = namespace_scope ? namespace_scope->FindVarSymbol(postfix_op->Name.get()) : nullptr;
    return {sym, namespace_scope};
  }

  // Identifiers or non-symbolic expressions can use the
  // normal lookup.
  auto sym = FindVarSymbol(adjusted_name->To<IdentifierAst>());
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

auto Scope::GetFinalChildScope() const -> Scope const* {
  // If there are no children, return this scope (base case
  // for the recursion). Otherwise, return the final child
  // scope (recursively searching).
  return Children.IsEmpty()
    ? this
    : Children.Back()->GetFinalChildScope();
}

auto Scope::GetAncestors() const -> Vec<Scope const*> {
  // Get all ancestor scopes, including this scope, and the
  // global scope.
  auto scopes = Vec<Scope const*>();
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    scopes.EmplaceBack(scope);
  }
  return scopes;
}

auto Scope::GetParentModule() const -> Scope* {
  // Get the parent module scope, if it exists.
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    if (std::holds_alternative<ScopeIdentifierName>(scope->Name)) {
      return const_cast<Scope*>(scope); // TODO: REMOVE CONST CAST
    }
  }
  return nullptr;
}

auto Scope::GetTopLevelParentModule() const -> Scope* {
  // Get the top level parent module scope (ie until the
  // parent is the global scope).
  // The global scope is the one with no parent.
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    const auto next_scope = scope->Parent;
    if (next_scope != nullptr and next_scope->Parent == nullptr) {
      return const_cast<Scope*>(scope); // TODO: REMOVE CONST CAST
    }
  }
  return nullptr;
}

auto Scope::FindEnclosingTypeScope(
  CompilerMetaData const &meta) const -> Scope* {
  // A closure's body scope is reparented to its module: use the scope it overrode.
  if (AstAs<ClosureExpressionAst>(AstNode) != nullptr and meta.OverriddenScopeForClosure != nullptr) {
    return meta.OverriddenScopeForClosure->FindEnclosingTypeScope(meta);
  }

  // Walk up the scope chain. Return the first type scope that's
  // non-compiler-generated, or the LinkedScope of a "Self" type
  // symbol found in a sup-block scope (for module-level sup
  // blocks that have no type scope in their chain).
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    if (scope->LinkedTypeSymbol != nullptr and not scope->LinkedTypeSymbol->IsMock()) {
      return const_cast<Scope*>(scope);
    }

    for (auto const &ty_sym : scope->InternalTable.TypeTable.GetAll()) {
      if (ty_sym->IsSelf() and ty_sym->LinkedScope != nullptr) {
        return ty_sym->LinkedScope;
      }
    }
  }
  return nullptr;
}

auto Scope::FindEnclosingSelfType(
  CompilerMetaData const &meta) const -> Shared<TypeAst> {
  // If we are already in a module scope, there is no self
  // type.
  auto current_scope = this;
  if (std::holds_alternative<ScopeIdentifierName>(current_scope->Name)) {
    return nullptr;
  }

  // Escape closure scopes for the Self type. Prefer using
  // the "Self" symbol type first.
  const auto is_closure_outer = [](Scope const *scope) {
    return AstAs<ClosureExpressionParameterAndCaptureGroupAst>(scope->AstNode) != nullptr;
  };
  if (is_closure_outer(current_scope) and meta.OverriddenScopeForClosure != nullptr) {
    current_scope = meta.OverriddenScopeForClosure;
  }
  if (const auto self_sym = current_scope->FindSelfSymbol();
    self_sym != nullptr and self_sym->LinkedSymbol() != self_sym) {
    // Inside a template, "Self" is the template over its own
    // parameters ("TypeSymbol::GnSelfName").
    return self_sym->LinkedSymbol()->GnSelfName();
  }

  // Use a "seen" walker to prevent scope searching cycles due
  // to nested closures, whose inner/outer scopes don't follow
  // normal scoping hierarchies.
  auto seen = Set<Scope const*>();
  while (true) {
    if (not seen.insert(current_scope).second) { return nullptr; }

    // Escape closure scopes for the Self type.
    if (is_closure_outer(current_scope) and meta.OverriddenScopeForClosure != nullptr) {
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

auto Scope::GetSupScopes() const
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
    for (const auto sup_scope : from.GetDirectSupScopes()) {
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

auto Scope::GetSupTypes() const -> Vec<Shared<TypeAst>> {
  // Get all the sup-scopes (cls/sup scopes) superimposed over
  // a type, filter to keep the class ones, and resolve (extract)
  // names properly. Collect and return. This uses "GetSupScopes" so
  // utilises an internal cache.
  auto ts = GetSupScopes()
    | genex::views::filter([](const auto scope) { return AstAs<ClassPrototypeAst>(scope->AstNode); })
    | genex::views::transform(SupTypeNameOf)
    | genex::views::filter([](auto const &type) { return type != nullptr; }) // Todo: shouldn't need.
    | genex::to<Vec>();
  return ts;
}

auto Scope::FindNsScope(
  Vec<IdentifierAst const*> const &parts) const -> Scope const* {
  auto const *scope = this;
  for (auto const *part : parts) {
    auto const *const sym = scope->FindNsSymbol(part);
    if (sym == nullptr) { return nullptr; }
    scope = sym->LinkedScope;
  }
  return scope;
}

auto Scope::ConvertPostfixToNestedScope(
  ExpressionAst const *postfix_ast) const -> Scope const* {
  // "a::b::c" holds "c" outermost: collected innermost first, then reversed.
  auto parts = Vec<IdentifierAst const*>();
  auto const *lhs = postfix_ast;
  while (auto const *postfix_lhs = lhs->To<PostfixExpressionAst>()) {
    parts.EmplaceBack(postfix_lhs->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>()->Name.get());
    lhs = postfix_lhs->Lhs.get();
  }
  if (auto const *const lhs_as_ident = lhs->To<IdentifierAst>(); lhs_as_ident != nullptr) {
    parts.EmplaceBack(lhs_as_ident);
  }
  parts |= genex::actions::reverse;
  return FindNsScope(parts);
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
