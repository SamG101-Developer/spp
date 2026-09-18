module;
#include <spp/macros.hpp>

export module spp.analyse.utils.drop_utils;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::drop_utils {
  /// Find the "drop" method superimposed over the type via
  /// the "std::ops::drop::Drop" type (either directly or via
  /// a base class being extended). Nullptr for when a drop
  /// method isn't provided (std::mem::ops::drop fallback).
  SPP_EXP_FUN auto FindDropOverload(
    TypeSymbol const &type_sym,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> FunctionPrototypeAst*;

  /// Whether a type needs dropping or not - if is has a drop
  /// method of its own, or contains a fields that does. Some
  /// primitive type wrappers - integers, bools, structs of
  /// them - destroy to nothing, so have no drop code. Borrows
  /// are not dropped through either, only owned values.
  SPP_EXP_FUN auto NeedsDrop(
    TypeSymbol const &type_sym,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> bool;

  /// When a value is being dropped, ensure that the drop
  /// method and all of its fields' drop methods are available
  /// (have been generated). The standard tree walk may have not
  /// reached it yet when needed. Banning recursive types in
  /// semantic analysis means we will never get a cycle here
  /// either.
  SPP_EXP_FUN auto EnsureDropInstantiated(
    TypeSymbol const &type_sym,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;
}
