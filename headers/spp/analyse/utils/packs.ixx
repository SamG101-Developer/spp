module;
#include <spp/macros.hpp>

export module spp.analyse.utils.packs;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::packs {
  /// Whether a type symbol is a type pack not yet bound to its
  /// tuple: a variadic generic parameter ("Ts" for "..Ts").
  SPP_EXP_FUN auto IsTypePack(
    TypeSymbol const &sym) -> bool;

  /// Whether a variable symbol is a comp pack not yet bound
  /// to its tuple: a variadic parameter ("ns" for "..ns: Str").
  SPP_EXP_FUN auto IsCompPack(
    VariableSymbol const &sym) -> bool;

  /// The element types of a type pack, which is bound to a tuple
  /// of them ("Ts=(S32, Bool)", or a comp pack's value typed
  /// "(Bool, Bool)"), in order. Empty for an empty pack. Todo:
  /// should this be applicable for comps?
  SPP_EXP_FUN auto PackElementTypes(
    TypeAst const &pack) -> Vec<Shared<TypeAst>>;

  /// The element values of a comp pack, which is bound to a tuple
  /// of them ("ns=(1_uz, 2_uz)"), in order: "PackElementTypes"
  /// for a comp pack. Empty for anything that is not a tuple.
  SPP_EXP_FUN auto PackElementValues(
    ExpressionAst const &pack) -> Vec<ExpressionAst*>;

  /// The element types a type naming a bound type pack stands for
  /// ("Ts" bound to "Tup[Bool, U8]" is "Bool, U8"), to be spread where
  /// a pack's elements are listed; nothing for anything else (an
  /// unbound pack, or one forwarded to another pack).
  SPP_EXP_FUN auto BoundPackTypes(
    TypeAst const &type, Scope const &scope) -> std::optional<Vec<Shared<TypeAst>>>;

  /// "BoundPackTypes" for a comp pack: the element values an
  /// identifier naming a bound comp pack stands for ("ns" bound to
  /// "(1_uz, 2_uz)").
  SPP_EXP_FUN auto BoundPackValues(
    ExpressionAst const &value, Scope const &scope) -> std::optional<Vec<ExpressionAst*>>;

  /// Whether a generic argument names a pack still unbound where it is
  /// read - a pattern's own variadic parameter, of either kind ("Rest"
  /// for "..Rest", "rest" for "cmp ..rest"), which takes whatever is
  /// left of the list opposite it.
  SPP_EXP_FUN auto IsUnboundPackNamed(
    GenericArgumentAst const &arg,
    Scope const &scope)
    -> bool;

  /// "IsUnboundPackNamed" for a comp value: an identifier naming an
  /// unbound comp pack.
  SPP_EXP_FUN auto IsUnboundCompPackNamed(
    ExpressionAst const &value, Scope const &scope) -> bool;

  /// Whether a parameter, by its "ParamId", is variadic: a type pack
  /// ("..Ts") or a comp pack ("cmp ..ns").
  SPP_EXP_FUN auto IsPackParam(
    std::uint64_t param_id) -> bool;

  /// The name of the type parameter a variadic function parameter
  /// is instantiated over ("..xs: S32" binds "VariadicPackOfxs" to
  /// the tuple a call passes). The one place the name is spelled.
  SPP_EXP_FUN auto PackTypeParamName(
    IdentifierAst const &param_name) -> Str;

  /// Whether a symbol is a pack not yet bound to its tuple: a
  /// variadic comp parameter or function parameter in a template.
  /// There its type is one element's ("Bool" for "..n: Bool"), and
  /// "n.0" reads as an element; in an instantiation it is the
  /// tuple itself, read like any other. Todo: Type args? Otherwise
  /// move into the single usage of it.
  SPP_EXP_FUN auto IsUnboundPack(
    VariableSymbol const &sym, Scope const &scope) -> bool;

  /// Whether a generic argument names a pack: a variadic type
  /// parameter ("Ts" for "..Ts") or comp parameter ("n" for
  /// "cmp ..n"), or in an instantiation the binding of one. Such
  /// an argument already is the tuple a pack parameter takes.
  SPP_EXP_FUN auto NamesPack(
    GenericArgumentAst const &arg, Scope const &scope) -> bool;
}
