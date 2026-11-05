module;
#include <spp/macros.hpp>

export module spp.analyse.utils.packs;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

namespace spp::analyse::utils::packs {
  /// What a call gives a function pack: the types of its elements, in order, conventions stripped (a pack's convention
  /// is its parameter's). A template forwarding its own pack ("f(x, ..b)", "b: Tup[Ts]") gives that pack's list after
  /// its own elements ("A, Ts"), which leaves it "Open": its length is still unknown.
  SPP_EXP_CLS struct GivenPack {
    Vec<Shared<TypeAst>> Elems;
    bool Open = false;
  };

  /// The elements of a comp pack bound to a tuple ("ns" bound to "(1_uz, 2_uz)"), borrowed from it: a tuple owns its
  /// elements alone.
  SPP_EXP_FUN auto CompPackElements(
    ExpressionAst const &pack) -> Vec<ExpressionAst*>;

  /// The elements an identifier naming a bound comp pack stands for, to be spread where a pack's elements are listed.
  /// Nothing for anything else (an unbound pack). A bound type pack is its list, spliced where it is keyed
  /// ("scopes::PackIdOf").
  SPP_EXP_FUN auto BoundCompPackElements(
    ExpressionAst const &value, Scope const &scope) -> Opt<Vec<ExpressionAst*>>;

  /// Whether a type names a pack still unbound where it is read: followed through its bindings to other generics, it
  /// is bound to nothing ("Rest" for "..Rest", or an argument bound to it), so it takes whatever is left of the list
  /// opposite it.
  SPP_EXP_FUN auto DoesTypeNameAnUnboundPack(
    TypeAst const &type, Scope const &scope) -> bool;

  /// The name of the type parameter a variadic function parameter is instantiated over ("..xs: S32" binds
  /// "VariadicPackOfxs" to the list of the types a call passes). The one place the name is spelled. No "comp"
  /// equivalent because this is for the types of the variadic function parameter, not a variadic generic type
  /// parameter.
  SPP_EXP_FUN auto FnVariadicParamName(
    IdentifierAst const &param_name) -> Str;

  /// Declare, in "scope", an unbound variadic type parameter standing for a pack whose length a template does not know:
  /// a pack of one element type's ("VariadicPackOf<xs>"), or a call fold's results over an open pack. Each element is
  /// "element", so the parameter derives from it (as copyable, zero-sized and thread-safe as it) and is held to what it
  /// meets: a generic's constraints, else the element itself, named as resolved here ("Self" the owner). "key" mints
  /// its identity once ("scopes::GnPackParamId"). Returns its name, stamped with that identity.
  SPP_EXP_FUN auto DeclareHiddenPack(
    Str const &name, TypeAst const &element, void const *key, std::size_t pos, Scope &scope)
    -> Shared<TypeIdentifierAst>;

  /// The name of the parameter an instance has for a closed pack's "index"th element ("xs#0"): no name a user can
  /// write, and as long as "xs.0", which it stands for. The one place it is spelled.
  SPP_EXP_FUN auto PackElementName(
    IdentifierAst const &pack_name, std::size_t index) -> Str;
}
