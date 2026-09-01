module;
#include <spp/macros.hpp>

export module spp.analyse.utils.drop_utils;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
  SPP_EXP_CLS struct FunctionPrototypeAst;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::analyse::utils::drop_utils {
  /**
   * The @c drop prototype that destroying a value of this type must call, which is the one the type gets from
   * superimposing @c std::ops::drop::Drop - directly, or through a type it extends. A type that superimposes @c Drop
   * without overriding @c drop resolves to the abstract prototype, which has an empty body and is not worth calling;
   * that case answers @c nullptr as well.
   * @param type_sym The symbol of the type being destroyed.
   * @param sm The scope manager, positioned anywhere the type resolves from.
   * @param meta Associated metadata.
   * @return The prototype to call, or @c nullptr if this type has no destructor of its own.
   */
  SPP_EXP_FUN auto FindDropOverload(
    scopes::TypeSymbol const &type_sym,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> asts::FunctionPrototypeAst*;

  /**
   * Whether destroying a value of this type does anything at all: it has a @c drop of its own, or it holds - at any
   * depth - an attribute that does. Everything else (@c S32 , @c Bool , a struct of them) destroys to nothing, and no
   * drop code is emitted for it. Borrowed values are not dropped through.
   * @param type_sym The symbol of the type in question.
   * @param sm The scope manager, positioned anywhere the type resolves from.
   * @param meta Associated metadata.
   * @return Whether dropping a value of this type has any effect.
   */
  SPP_EXP_FUN auto NeedsDrop(
    scopes::TypeSymbol const &type_sym,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> bool;

  /**
   * Mint the instantiations that destroying a value of this type will need, for the whole tree @c codegen::EmitDrop
   * will walk: the type's own @c drop , or, when it has none, the @c drop of every attribute that has one, and so on
   * down.
   * @param type_sym The symbol of the type being destroyed.
   * @param sm The scope manager, positioned anywhere the type resolves from.
   * @param meta Associated metadata.
   */
  SPP_EXP_FUN auto EnsureDropInstantiated(
    scopes::TypeSymbol const &type_sym,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> void;
}
