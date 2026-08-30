module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_predicates;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct ClassPrototypeAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::analyse::utils::type_predicates {
  SPP_EXP_FUN auto IsTypeCompTimeIndexable(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeSelf(
    asts::TypeAst const &type)
    -> bool;

  SPP_EXP_FUN auto IsTypeArr(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeTup(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTupSymbol(
    scopes::TypeSymbol const &sym)
    -> bool;

  SPP_EXP_FUN auto IsTypeVariant(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeBool(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeVoid(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  /**
   * Whether @p type is @c std::generator::Gen or @c std::generator::GenOnce - a handle to a coroutine frame. Only the
   * type itself is considered, not anything it superimposes.
   */
  /**
   * Check if a type is a "Try" type, ie "std::try::Try[Ok, Err]" or anything superimposing it. Used by "GetTryType" to
   * pick the try type out of a type's super types.
   * @param type The type to check.
   * @param scope The scope to check the type in.
   * @return If the type is a try type.
   */
  SPP_EXP_FUN auto IsTypeTry(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeGen(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeNever(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeFunc(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  /**
   * The number of synthetic fat-pointer fields ("resume_fn"/"env_ptr", or "fn_ptr"/"env_ptr") prepended ahead of
   * @p type's own declared attributes, because @p type superimposes one of the "IsTypeFatPointerFamily" types.
   * Zero if it doesn't superimpose one. See "IsTypeFatPointerFamily".
   * @param type The type to test.
   * @param scope The scope to resolve @p type against.
   * @return The fat pointer prefix's field count (0 or 2).
   */
  SPP_EXP_FUN auto GetSuperimposedFatPointerFieldCount(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> std::size_t;

  SPP_EXP_FUN auto IsTypeRecursive(
    asts::ClassPrototypeAst const &type,
    scopes::ScopeManager const &sm)
    -> Shared<asts::TypeAst>;

  /**
   * Whether a type names real types the whole way down: itself, and every generic argument inside it, however deeply
   * nested. This is what separates a genuine instantiation from a template wearing one's clothes - @c "NonNull[T=T]"
   * or @c "SizedInteger[w=w]" - which is registered while a generic body is analysed and must never be laid out or
   * emitted.
   *
   * Asking whether the type @e lowers is not the same question and does not answer this one: a borrowed type lowers to
   * a pointer whatever it points at, and @c NonNull lowers to a bare pointer whatever it holds, so both report success
   * for an argument that is still a parameter.
   *
   * @param[in] type The type to inspect.
   * @param[in] scope The scope to resolve @p type and its arguments against - the instantiation's own, never the
   * caller's, since a caller may have a same-named parameter bound to something real.
   * @return Whether every name in @p type resolves to a class rather than to an unbound parameter.
   */
  SPP_EXP_FUN auto IsTypeFullyConcrete(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool;

  SPP_EXP_FUN auto IsTypeBorrowed(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm,
    bool deep = true)
    -> bool;

  SPP_EXP_FUN auto IsIndexWithinBound(
    std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Pair<bool, std::size_t>;

  SPP_EXP_FUN auto GetNthTypeOfIndexableType(
    std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Shared<asts::TypeAst>;

  SPP_EXP_FUN auto AreGenericArgsConcrete(
    Vec<Unique<asts::GenericArgumentAst>> const &args,
    scopes::Scope const &scope)
    -> bool;
}
