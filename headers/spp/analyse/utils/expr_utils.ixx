module;
#include <spp/macros.hpp>

export module spp.analyse.utils.expr_utils;
import spp.utils.types;
import sys;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct StatementAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::expr_utils {
  SPP_EXP_CLS struct PrimaryExpressionOptions {
    bool AllowTypeAst = false;
  };

  /// Validate whether the use of a certain "primary expression"
  /// ast is genuinely valid in context. For example, ".." is
  /// valid in binary expressions ie ".. + tup", but not say as
  /// "f(..)".
  SPP_EXP_FUN auto IsPrimaryExprTypeValid(
    ExpressionAst const &expr,
    ScopeManager const &sm,
    PrimaryExpressionOptions &&options = {})
    -> bool;

  /// Prevent statements/expressions from being used whose
  /// value is unbound and falls away. For example, just
  /// writing "1" in a function body is wrong. It is never
  /// required and often the source of bugs; prevent at
  /// compile time.
  SPP_EXP_FUN auto ValidateDiscardedValue(
    Ast &member,
    Scope *scope,
    ScopeManager const &sm,
    CompilerMetaData *meta)
    -> void;

  /// Whether control never continues past a statement: it is
  /// written to leave ("ret", "exit", "skip", or a block/case
  /// every path of which does), or its analysed type is "!" (a
  /// call to "abort", a loop with no way out). A "let" diverges
  /// when its value does. The statement must be analysed.
  SPP_EXP_FUN auto Diverges(
    StatementAst &stmt, ScopeManager *sm, CompilerMetaData *meta) -> bool;
}
