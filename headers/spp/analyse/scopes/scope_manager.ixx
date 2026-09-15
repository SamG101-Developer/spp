module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope_range;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct LoopExpressionAst);
use(spp::asts, struct ModulePrototypeAst);
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
  /// symbol that is linked to the scope of the enclosing
  /// class or superimposition block. Effectively a placeholder
  /// ast until resolution is performed.
  Unique<ClassPrototypeAst> _SelfProto;

public:
  /// The static list of sup blocks that have been processed.
  /// This field is defined as static so that all the scope
  /// managers (including the temporary ones) can share it.
  inline static Map<TypeSymbol*, Vec<Scope*>> normal_sup_blocks = {};

  /// The generic sup blocks are sup blocks that will be
  /// applied to all types, and have some special handling
  /// required. Example: "sup [T] T { ... }" and "ext" too.
  /// Todo: Do constraints work with this?
  inline static Vec<Scope*> generic_sup_blocks = {};

  /// The temp scopes are a holder of scopes that are generated
  /// for certain things (closures, generic parameter types)
  /// and just need somewhere to live.
  /// Todo: Should look at being able to pop from this during
  /// the compilation (when a temp scope is no longer needed etc).
  inline static Vec<Unique<Scope>> temp_scopes = {};

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

  /// Provide the access to the the scope iterator, by
  /// creating a range whose "begin()" and "end()" return
  /// scope iterator instantiations containing the current
  /// scope and nullptr (sentinel).
  SPP_ATTR_NODISCARD auto Iter() const -> ScopeRange;

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
  /// of the current scope - the final scope that will be the
  /// new current scope is the "FinalChildScope". One more
  /// iteration and the immediate sibling to this scope will be
  /// reached.
  auto ExhaustScope() -> void;

  /// The deferment logic is explained below, but this struct
  /// tracks the information needed to accurately defer the
  /// constraint checking and subsequent potential pruning.
  struct DeferredSupConstraint {
    /// The scope the super scope was attached to (pruned if
    /// the constraint fails).
    Scope *owner_scope;

    /// The (specialized) super scope that was attached.
    Scope *sup_scope;

    /// The paired super class scope also attached, or nullptr.
    Scope *sup_cls_scope;

    /// The original (constrained) generic sup block.
    Scope *base_sup_scope;
  };

  /// For every type discovered upto this point, attach the
  /// designated supertypes to them. By this point, all the
  /// base classes and sup blocks will have been added to the
  /// symbol tables, but not all the generic instantiation of
  /// these types. A 2-phase attachment process happens,
  /// allowing for order-agnostic attaching. The first phase
  /// attaches every possible superscope for the type, and
  /// defers any generic constraint check. This is because the
  /// constraint might not have been built / sup-attached yet;
  /// this would be order dependent. The second phase then
  /// validates the constraints, and prunes off the unsatisfied
  /// constraints. This is repeated to a fixpoint so transitive
  /// constraint chains resolve.
  auto AttachAllSuperScopes(CompilerMetaData *meta) -> void;

  /// The public method to attach all the super scopes to an
  /// individual type. This is called repeatedly by the attach
  /// all super scopes function, but also individually hit by
  /// stage 7 monomorphisation.
  auto AttachSpecificSuperScopes(
    Scope &scope,
    CompilerMetaData *meta,
    Vec<DeferredSupConstraint> *deferred = nullptr) const
    -> void;

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
    CompilerMetaData *meta,
    Vec<DeferredSupConstraint> *deferred) const
    -> void;

  /// Perform the pruning when constraints don't match. This
  /// is the second stage of the 2-phase sup-scope attachment
  /// strategy. Repeated to a "fixed point", because pruning
  /// one attachment can invalidate the constraint of another
  /// (transitive constraint chains).
  auto PruneUnsatisfiedSupConstraints(Vec<DeferredSupConstraint> &deferred, CompilerMetaData *meta) const -> void;

  /// Once a super-scope has been registered against a scope,
  /// there are some post-attachment steps that need to be run.
  /// There can be no conflicting cmp or type statements with
  /// existing definitions on the existing super types of the
  /// type.
  static auto CheckConflictingTypeOrCmpStatements(TypeSymbol const &cls_sym, Scope const &sup_scope) -> void;

public:
  /// Get the current iterator, generally for copying into a
  /// scope manager clone.
  auto CurrentIterator() -> ScopeIterator&;

  /// Get the placeholder self prototype. Todo: Might see if
  /// this can be removed.
  auto SelfProto() const -> ClassPrototypeAst*;

  /// Add the symbol for the special "Self" type, providing
  /// the linked scope for it to map against. Done as early
  /// as possible, making "Self" resolvable.
  auto AddSelfTypeSymbol(Scope *linked_scope, std::size_t pos) const -> void;

  /// Clear the static caches on the scope manager for general
  /// cleanup. Called at the end of a compilation.
  static auto Cleanup() -> void;
};
