module;
#include <spp/macros.hpp>

export module spp.analyse.utils.generic_inference;
import spp.analyse.scopes.instance_key;
import spp.analyse.utils.type_compare;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::utils::generic_inference, class GenericSolver);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

/// Solves the generic arguments of one use of a generic declaration: a call of a generic function, or a generic
/// type named with (or without) its arguments. Everything a parameter is offered is collected against that parameter
/// - its declaration, not its spelling - and reconciled once:
///
/// 1. What is given, in layers of precedence ("Give"): the arguments written at the use site, then whatever the
///    receiver, the enclosing "sup" block and a pinned "Self" bind. A name already given keeps its first binding.
/// 2. Equations ("Unify"): what was given for a parameter or attribute against its declared type, binding the
///    generics the declared type names.
/// 3. "Solve": the equations, then comp values' types and the constraints, repeated until nothing new is bound; then
///    the defaults; then conflicts and uninferred parameters are checked, every binding is read in the use site's
///    terms, and both kinds of argument are checked against their parameters (comp types, type constraints).
/// 4. "TakeArgs": the solution, in parameter order.
SPP_EXP_CLS class spp::analyse::utils::generic_inference::GenericSolver {
public:
  GenericSolver(
    GenericParameterGroupAst const &params,
    Scope const &owner_scope,
    ScopeManager &sm,
    meta::CompilerMetaData &meta);

  GenericSolver(GenericSolver const &) = delete;
  ~GenericSolver();
  auto operator=(GenericSolver const &) -> GenericSolver& = delete;

  /// Take in a layer of named arguments, below every layer already
  /// given. "emit" says whether a name that is not one of the
  /// parameters ("Self") is part of the solution, or only seen by
  /// the inference.
  auto Give(
    Vec<Unique<GenericArgumentAst>> args,
    bool emit = false)
    -> void;

  /// Record that "source" was given where "target" is declared,
  /// under the name "name" (a parameter or an attribute).
  auto Unify(
    Shared<IdentifierAst> const &name,
    Shared<TypeAst> source,
    Shared<TypeAst> target)
    -> void;

  /// Everything bound so far, as arguments (borrowed from the solver).
  SPP_ATTR_NODISCARD auto GetKnownArgs() const
    -> Vec<GenericArgumentAst*>;

  /// Solve, raising on a conflict, an uninferred parameter, a comp
  /// argument of the wrong type or an unsatisfied constraint. "variadic_fn_param" names a
  /// variadic function parameter, whose non-variadic generics bind
  /// to the pack's element rather than its tuple.
  auto Solve(
    Ast const &owner,
    Shared<IdentifierAst> const &variadic_fn_param = nullptr)
    -> void;

  /// The solution, in parameter order, followed by the emitted
  /// names that are not parameters.
  auto TakeArgs()
    -> Vec<Unique<GenericArgumentAst>>;

private:
  struct _Entry;
  struct _Equation;

  GenericParameterGroupAst const *_Params;
  Scope const *_OwnerScope;
  ScopeManager *_Sm;
  meta::CompilerMetaData *_Meta;
  Vec<Unique<_Entry>> _Entries;
  Vec<Unique<_Equation>> _Equations;
  Vec<Unique<GenericArgumentAst>> _Given;

  bool _Trivial = false;

  auto _Find(TypeIdentifierAst const &name) const -> _Entry*;
  auto _OfferAll(type_compare::GenericInferenceMap const &inferred, Function<bool(_Entry const &)> const &skip,
    Function<Shared<TypeAst>(_Entry const &, Shared<TypeAst>)> const &adjust_type = nullptr) -> bool;
  auto _Match(Shared<TypeAst> const &source, Shared<TypeAst> const &target) const
    -> type_compare::GenericInferenceMap;
  auto _ReadCompValues() -> bool;
  auto _ReadConstraints() -> bool;
  auto _SelfForDefault(bool as_value) const -> Shared<TypeAst>;
  auto _ApplyDefaults() -> void;
  auto _EnforceNoConflicts() const -> void;
  auto _EnforceAllInferred(Ast const &owner) const -> void;
  auto _CrossSubstitute() -> void;
  auto _CheckTypeArgs(scopes::GenericSubst const &bindings) const -> void;
  auto _CheckCompArgs(scopes::GenericSubst const &bindings) const -> void;
  auto _InferenceMap() const -> type_compare::GenericInferenceMap;
};

namespace spp::analyse::utils::generic_inference {
  /// A sup block's or an alias's own generic parameters, standing
  /// in as the arguments to the type they fill ("sup [T] Box[T]"),
  /// checked against that type's constraints.
  SPP_EXP_FUN auto EnforceGnConstraintsOfParams(
    TypeSymbol const &target,
    GenericParameterGroupAst const &params,
    ScopeManager &sm,
    meta::CompilerMetaData &meta)
    -> void;

  /// The arguments written for "p_group", each named after the
  /// parameter it binds, in parameter order: a positional argument
  /// binds the next parameter not named by a keyword one, and a
  /// trailing variadic parameter takes the rest as a tuple (a lone
  /// argument naming a pack already is that tuple). "written" is
  /// not changed. A tuple's arguments stay positional.
  SPP_EXP_FUN auto NamedGnArgs(
    GenericArgumentGroupAst const &written,
    GenericParameterGroupAst const &p_group,
    Ast const &owner,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    bool is_tuple_owner = false)
    -> Unique<GenericArgumentGroupAst>;

}
