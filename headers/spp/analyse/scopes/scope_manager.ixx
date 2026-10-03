module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_iterator;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct LoopExpressionAst);
use(spp::asts, struct ModulePrototypeAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeStatementAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::utils::errors, class ErrorFormatter);

/// The scope manager type is the most critical structure of
/// the entire analysis engine. It contains the entire scope
/// tree of the code, providing the overarching structure,
/// and the symbols within those scopes. It also provides
/// scope iteration capability (depth-first).
SPP_EXP_CLS class spp::analyse::scopes::ScopeManager {
  /// The current iterator state. This is a customised
  /// iterator for depth-first searching. Use the "reset"
  /// method to push the iterator back to a specific scope,
  /// or nullptr for the root scope.
  ScopeIterator _It;

  /// The "Self" type symbol, which is a special type

public:
  /// The static list of sup blocks that have been processed.
  /// This field is defined as static so that all the scope
  /// managers (including the temporary ones) can share it.
  inline static Map<TypeSymbol*, Vec<Scope*>> NormalSupBlocks = {};

  /// The generic sup blocks are sup blocks that will be
  /// applied to all types, and have some special handling
  /// required. Example: "sup [T] T { ... }" and "ext" too.
  /// Todo: Do constraints work with this?
  inline static Vec<Scope*> GnSupBlocks = {};

  /// The temp scopes are a holder of scopes that are generated
  /// for certain things (closures, generic parameter types)
  /// and just need somewhere to live.
  /// Todo: Should look at being able to pop from this during
  /// the compilation (when a temp scope is no longer needed etc).
  inline static Vec<Unique<Scope>> TempScopes = {};

  /// The master root global scope, the root of the entire
  /// compilation tree. This is shared into temp scope managers.
  Shared<Scope> GlobalScope;

  /// The current scope is the scope that has been iterated
  /// to, and stays consistent with the ast it is being used
  /// for between stages of the compiler.
  Scope *CurrentScope = nullptr;

  /// Create a new scope manager, providing a global scope and
  /// an optional current scope. A nullptr current scope sets
  /// the current scope to the global scope.
  explicit ScopeManager(
    Shared<Scope> const &global_scope,
    Scope *current_scope = nullptr);

  /// Default destructor logic.
  ~ScopeManager();

  /// Reset the scope manager to the provided scope. If the
  /// scope it nullptr, then it will reset to the global scope.
  /// The internal iterator is reset too. A partially iterated
  /// iterator can be provided in the case that we want to
  /// start from a designated scope.
  /// Todo: Check all callers and decide on signature.
  auto Reset(Scope *scope = nullptr, std::optional<ScopeIterator> iterator = std::nullopt) -> void;

  /// Create a new scope and move into it. It will be the child
  /// of the current scope, and the internal iterator will
  /// update to point into it on iteration. Because of how the
  /// iterator works, "live updates" are fine and the next
  /// scope is this new one. The name of the scope is provided
  /// here, alongside an optional error formatter.
  auto CreateAndMoveIntoNewScope(
    ScopeName const &name,
    Ast *ast = nullptr,
    ErrorFormatter *error_formatter = nullptr)
    -> Scope*;

  /// Move out of the current scope and into the parent scope.
  /// This is used after a scope has been moved into for a
  /// function or class. Returns the parent scope too.
  auto MoveOutOfCurrentScope() -> Scope*;

  /// Iterate into the next scope without creating a new scope.
  /// This just increments the iterator, and skips alias class
  /// scopes so that each ast sync properly.
  auto MoveToNextScope(bool ignore_alias_class_scopes = true) -> Scope*;

  /// Advance the iterator past every scope that's a descendant
  /// of the current scope, up to its "GetFinalChildScope", from
  /// wherever in that subtree the walk has got to. One more
  /// iteration and the immediate sibling to this scope will be
  /// reached. The current scope stays the one exhausted, so that
  /// "MoveOutOfCurrentScope" leaves it for its own parent.
  auto ExhaustScope() -> void;

  /// Iterate the tree walker until we are on the last scope
  /// of the inputted "scope"'s scope tree. This is needed, so
  /// that when collecting multiple errors, we can go "this
  /// function threw", now skip past it, onto the next function.
  auto SkipPastScope(Scope const *scope) -> bool;

  /// For every type discovered up to this point, attach the
  /// designated supertypes to it, checking each generic
  /// constraint as it is attached. A constraint is checked
  /// against the constrained type's own supertypes, which may
  /// not be attached yet, so reading a scope's supertypes during
  /// the sweep attaches them first ("Scope::OnSupScopesRead").
  auto AttachAllSuperScopes(CompilerMetaData *meta) -> void;

  /// The public method to attach all the super scopes to an
  /// individual type, once ("Scope::SupsAttached"), returning
  /// whether it attached them now. Called by the attach all
  /// super scopes function, and individually when a generic
  /// class is instantiated.
  auto AttachSpecificSuperScopes(
    Scope &scope,
    CompilerMetaData *meta) const
    -> bool;

private:
  /// Given a method's "$", coalesce the overloads to this type,
  /// say "$MyFunc" from all the different sup blocks containing
  /// an overload of "my_func" for a specific type, so they all
  /// see the full set of overloads.
  auto CoalesceMethodMock(Scope &scope) const -> void;

  /// The implementation logic for the sup scope attachment
  /// onto a type scope. This is separated out so the public
  /// api can so some logic on class vs alias scopes.
  auto AttachSpecificSuperScopesImpl(
    Scope &scope,
    Vec<Scope*> const &sup_scopes,
    CompilerMetaData *meta) const
    -> void;

  /// Once a super-scope has been registered against a scope,
  /// there are some post-attachment steps that need to be run.
  /// There can be no conflicting cmp or type statements with
  /// existing definitions on the existing super types of the
  /// type.
  static auto CheckConflictingTypeOrCmpStatements(TypeSymbol const &cls_sym, Scope const &sup_scope) -> void;

public:
  /// Get the current iterator, generally for copying into a
  /// scope manager clone.
  auto GetCurrentIterator() -> ScopeIterator&;

  /// Build the symbol for the special "Self" type, linked to the
  /// scope of the type it names and defined in "defined_in".
  static auto MakeSelfTypeSymbol(Scope *linked_scope, Scope *defined_in, std::size_t pos) -> Shared<TypeSymbol>;

  /// Add the symbol for the special "Self" type, providing
  /// the linked scope for it to map against. Done as early
  /// as possible, making "Self" resolvable.
  auto AddSelfTypeSymbol(Scope *linked_scope, std::size_t pos) const -> void;

  /// Point the current scope's "Self" symbol at the class "cls_name" names, taking that class's type and layout. A
  /// compiler-generated name (a method's "$" mock) is left alone.
  auto SyncSelfTypeSymbol(TypeAst const &cls_name) const -> void;

  /// Clear the static caches on the scope manager for general
  /// cleanup. Called at the end of a compilation.
  static auto Cleanup() -> void;
};
