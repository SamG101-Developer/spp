module;
#include <spp/macros.hpp>

export module spp.analyse.utils.packs;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::packs {
  /// The elements of a type pack bound to a tuple ("Ts" bound to "Tup[S32, Bool]" is "S32, Bool"), in order. Empty
  /// for an empty pack. Shared, as a type argument is: they outlive the tuple type, which is often built to be read.
  SPP_EXP_FUN auto TypePackElements(
    TypeAst const &pack) -> Vec<Shared<TypeAst>>;

  /// "TypePackElements" for a comp pack bound to a tuple ("ns" bound to "(1_uz, 2_uz)"), borrowed from it: a tuple
  /// owns its elements alone.
  SPP_EXP_FUN auto CompPackElements(
    ExpressionAst const &pack) -> Vec<ExpressionAst*>;

  /// The elements a type naming a bound type pack stands for, followed through its bindings to other generics, to be
  /// spread where a pack's elements are listed. Nothing for anything else (an unbound pack, or one bound to another).
  SPP_EXP_FUN auto BoundTypePackElements(
    TypeAst const &type, Scope const &scope) -> std::optional<Vec<Shared<TypeAst>>>;

  /// "BoundTypePackElements" for an identifier naming a bound comp pack.
  SPP_EXP_FUN auto BoundCompPackElements(
    ExpressionAst const &value, Scope const &scope) -> std::optional<Vec<ExpressionAst*>>;

  /// Whether a type names a pack still unbound where it is read: followed through its bindings to other generics, it
  /// is bound to nothing ("Rest" for "..Rest", or an argument bound to it), so it takes whatever is left of the list
  /// opposite it.
  SPP_EXP_FUN auto DoesTypeNameAnUnboundPack(
    TypeAst const &type, Scope const &scope) -> bool;

  /// "DoesTypeNameAnUnboundPack" for an identifier naming a comp pack ("rest" for "cmp ..rest").
  SPP_EXP_FUN auto DoesCompNameAnUnboundPack(
    ExpressionAst const &value, Scope const &scope) -> bool;

  /// Whether a generic argument names a pack: a variadic type parameter ("Ts" for "..Ts") or comp parameter ("n" for
  /// "cmp ..n"), or in an instantiation the binding of one. Such an argument already is the tuple a pack parameter
  /// takes.
  SPP_EXP_FUN auto DoesArgNameAPack(
    GenericArgumentAst const &arg, Scope const &scope) -> bool;

  /// Whether a generic argument names an unbound pack, of its kind ("DoesTypeNameAnUnboundPack",
  /// "DoesCompNameAnUnboundPack").
  SPP_EXP_FUN auto DoesArgNameAnUnboundPack(
    GenericArgumentAst const &arg, Scope const &scope) -> bool;

  /// The name of the type parameter a variadic function parameter is instantiated over ("..xs: S32" binds
  /// "VariadicPackOfxs" to the tuple a call passes). The one place the name is spelled.
  SPP_EXP_FUN auto PackTypeParamName(
    IdentifierAst const &param_name) -> Str;
}
