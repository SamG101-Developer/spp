module;
#include <spp/macros.hpp>

export module spp.asts.utils.generic_substitution;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
}

/**
 * Generic substitution over the shapes that carry types but are not types.
 *
 * @c TypeAst::SubstituteGenerics rewrites a type, which is enough wherever a signature is translated - a parameter's
 * type, a return type. It is not enough for the places an @e expression is carried out of the scope that wrote it,
 * because the types inside one are then read somewhere that has never heard of the names they are written in.
 */
namespace spp::asts::utils::generic_substitution {
  /**
   * Rewrite every type written inside an expression against a set of generic arguments, in place.
   *
   * @n
   * Only the shapes that can name a type are walked; a literal or a plain identifier names none and is left alone. An
   * expression form not handled here is not rewritten, so whatever it names stays as written - which surfaces where
   * that name fails to resolve, rather than silently producing the wrong type.
   *
   * @param[in,out] expr The expression to rewrite. Owned by its caller, so that a type sitting at the root can be
   * replaced outright rather than only rewritten in place.
   * @param[in] generic_args The bindings to rewrite against. An empty set leaves @p expr untouched.
   */
  SPP_EXP_FUN auto SubstituteGenericsInExpression(
    Unique<ExpressionAst> &expr,
    Vec<GenericArgumentAst*> const &generic_args)
    -> void;
}
