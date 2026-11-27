module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_ast;
import spp.analyse.scopes.scope;
import spp.asts.ast_kind;
import spp.asts.function_call_argument_positional_ast;
import spp.utils.types;
import std;

use_common(ClosureExpressionCaptureAst);

SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentPositionalAst {
  SPP_AST_KEY_FUNCTIONS(ClosureExpressionCaptureAst);

  ClosureExpressionCaptureAst(
    decltype(Conv) &&conv,
    decltype(Val) &&val);

  ~ClosureExpressionCaptureAst() override;

  /// True for a by-value capture of a non-copyable value, which
  /// moves the outer variable into the closure. A copyable value
  /// is copied, and a borrow moves nothing. The captured variable
  /// is looked up in "scope".
  SPP_ATTR_NODISCARD auto ConsumesCapture(analyse::scopes::Scope const &scope) const -> bool;
};
