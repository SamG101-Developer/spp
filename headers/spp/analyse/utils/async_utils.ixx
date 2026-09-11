module;
#include <spp/macros.hpp>

export module spp.analyse.utils.async_utils;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ClosureExpressionCaptureAst;
  SPP_EXP_CLS struct ConventionAst;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct StatementAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
}

namespace spp::analyse::utils::async_utils {
  /**
   * Add a capture, unless the variable is already captured. A variable is captured once, however many parts of the
   * call use it, under the strongest use: a move outranks @c &mut, which outranks @c &. So
   * @code f(&a.b, &mut a.c)@endcode captures @c a once, as @c &mut, and the closure's body uses its parts.
   * @param[in,out] captures The closure's captures so far.
   * @param[in] name The variable to capture.
   * @param[in] conv The convention to capture it under, or null to capture it by move.
   */
  SPP_EXP_FUN auto CaptureOnce(
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    Unique<asts::IdentifierAst> &&name,
    Unique<asts::ConventionAst> &&conv)
    -> void;

  /**
   * Capture what a borrow needs. A borrow of a place, @code &a.b.c@endcode or @code &a@endcode, is taken through the
   * place's outermost variable, so that variable is captured under the borrow's convention and the body borrows as
   * written. A temporary, @code &g(x)@endcode, has no variable, so it is bound to a local the closure owns and the body
   * borrows that instead. A static member resolves on its own, and needs nothing.
   * @param[in,out] place The borrowed expression. Replaced by a local's name if it is a temporary.
   * @param[in] conv The borrow's convention.
   * @param[in] scope The scope the @c async expression is in.
   * @param[in,out] prelude The bindings made before the closure.
   * @param[in,out] captures The closure's captures so far.
   * @param[in] pos The position given to generated names.
   */
  SPP_EXP_FUN auto CaptureBorrow(
    Unique<asts::ExpressionAst> &place,
    asts::ConventionAst const &conv,
    scopes::Scope const &scope,
    Vec<Unique<asts::StatementAst>> &prelude,
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    std::size_t pos)
    -> void;

  /**
   * Capture a method call's receiver, @c a.b in @code async a.b.c()@endcode, the way the method's @c self uses it -
   * exactly as an argument would be. @c &self and @c &mut self borrow it (see @c CaptureBorrow), and @c self moves it:
   * a bare name is captured, and anything else is bound to a local the closure owns.
   * @param[in,out] path The call's target, a runtime member access. Its receiver may be replaced by a local's name.
   * @param[in] target The method being called, or null if it is not known.
   * @param[in] scope The scope the @c async expression is in.
   * @param[in,out] prelude The bindings made before the closure.
   * @param[in,out] captures The closure's captures so far.
   * @param[in] pos The position given to generated names.
   */
  SPP_EXP_FUN auto CaptureReceiver(
    asts::PostfixExpressionAst &path,
    asts::FunctionPrototypeAst const *target,
    scopes::Scope const &scope,
    Vec<Unique<asts::StatementAst>> &prelude,
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    std::size_t pos)
    -> void;
}
