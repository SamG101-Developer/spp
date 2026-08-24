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
   * The @c del prototype that destroying a value of this type must call, which is the one the type gets from
   * superimposing @c std::ops::del::Del - directly, or through a type it extends. A type that superimposes @c Del
   * without overriding @c del resolves to the abstract prototype, which has an empty body and is not worth calling;
   * that case answers @c nullptr as well.
   * @param type_sym The symbol of the type being destroyed.
   * @param sm The scope manager, positioned anywhere the type resolves from.
   * @param meta Associated metadata.
   * @return The prototype to call, or @c nullptr if this type has no destructor of its own.
   */
  SPP_EXP_FUN auto FindDelOverload(
    scopes::TypeSymbol const &type_sym,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> asts::FunctionPrototypeAst*;

  /**
   * Whether destroying a value of this type does anything at all: it has a @c del of its own, or it holds - at any
   * depth - an attribute that does. Everything else (@c S32 , @c Bool , a struct of them) destroys to nothing, and no
   * drop code is emitted for it.
   *
   * @n
   * A borrowed type is never dropped through: a borrow does not own what it points at. Recursion terminates on that,
   * and on the fact that a type cannot contain itself by value.
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
}
