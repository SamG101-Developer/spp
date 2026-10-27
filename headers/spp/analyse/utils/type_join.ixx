module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_join;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::utils::type_join, class TypeJoin);
use(spp::analyse::utils::type_join, struct JoinedValue);
use(spp::asts, struct Ast);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::type_join {
  /// One value given to a join: where it is written (what an
  /// error points at), its type and the scope that type is read
  /// in, and whether the path it is on leaves before the join (a
  /// branch that returns), which makes it no value at all.
  SPP_EXP_CLS struct JoinedValue {
    Ast const *Site = nullptr;
    Shared<TypeAst> Type;
    Scope const *TypeScope = nullptr;
    bool Diverges = false;
  };
}

/// The one type many values agree on: a "case"'s branches, a
/// loop's "exit"s and its "else", a closure's "ret"s. It is the
/// type the whole is assigned to, when there is one ("target");
/// otherwise the widest variant among the values, which each
/// member widens into; otherwise the first value's. Every other
/// value must go into it ("Assignable"), or it is a type
/// mismatch (E1). A value on a path that diverges is no value:
/// when every path does, the ones typed "!" stand for the whole,
/// and nothing has to agree.
SPP_EXP_CLS class spp::analyse::utils::type_join::TypeJoin {
public:
  /// A join that the values agree on among themselves, or on
  /// "target" (read in "target_scope", written at "target_site")
  /// when the whole is assigned to it.
  explicit TypeJoin(
    Shared<TypeAst> target = nullptr, Scope const *target_scope = nullptr, Ast const *target_site = nullptr);

  /// Give the join a value, in the order the values are written.
  auto Give(
    Ast const &site, Shared<TypeAst> type, Scope const &type_scope, bool diverges)
    -> void;

  /// The agreed value: its type, and the value (or the target)
  /// it was taken from, marked "Diverges" when every value does.
  /// A type mismatch is raised for the first value that does not
  /// go into it. No type when none was given.
  SPP_ATTR_NODISCARD auto Solve() const
    -> JoinedValue;

  /// Instantiate the function named by each value as the
  /// overload "agreed" asks for ("fn_values::InstantiateFnValue").
  auto InstantiateFnValues(
    JoinedValue const &agreed, ScopeManager *sm, CompilerMetaData *meta) const
    -> void;

  /// Whether no value has been given.
  SPP_ATTR_NODISCARD auto Empty() const
    -> bool;

  /// Every value given, in order.
  SPP_ATTR_NODISCARD auto Values() const
    -> Vec<JoinedValue> const&;

private:
  /// What the whole is assigned to, if anything.
  JoinedValue _Target;

  /// Every value given, in order.
  Vec<JoinedValue> _Values;
};
