module;
#include <spp/macros.hpp>

export module spp.analyse.utils.async_utils;
import spp.utils.types;
import std;

use(spp::asts, struct ClosureExpressionCaptureAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct StatementAst);
use(spp::analyse::scopes, class Scope);

namespace spp::analyse::utils::async_utils {
  /// Add a capture to the closure lowered from the async call
  /// mapper, unless the variable is already captured. This is
  /// so that using "&a.b" and "&a.c" as the args to an async
  /// call, only captures "&a" once into the closure, and
  /// satisfies memory rules. Internal convention ranking handles
  /// different capture conventions.
  SPP_EXP_FUN auto CaptureOnce(
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    Unique<asts::IdentifierAst> &&name,
    Unique<asts::ConventionAst> &&conv)
    -> void;

  /// Determine what is being borrowed, and therefore what needs
  /// to happen for the closure. Borrowing a field needs to
  /// being in the outermost symbol, borrowing a temporary needs
  /// to materialize it etc.
  SPP_EXP_FUN auto CaptureBorrow(
    Unique<asts::ExpressionAst> &place,
    asts::ConventionAst const &conv,
    scopes::Scope const &scope,
    Vec<Unique<asts::StatementAst>> &prelude,
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    std::size_t pos)
    -> void;

  /// Capture a method calls receiver; for example in the expr
  /// "async a.b.c()" it'd be "a.b" ie what is "self"? The
  /// convention is handled too. This is needed to "capture" it
  /// as "self" - temporaries are materialized by binding to a
  /// closure-owned local.
  SPP_EXP_FUN auto CaptureReceiver(
    asts::PostfixExpressionAst &path,
    asts::FunctionPrototypeAst const *target,
    scopes::Scope const &scope,
    Vec<Unique<asts::StatementAst>> &prelude,
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    std::size_t pos)
    -> void;
}
