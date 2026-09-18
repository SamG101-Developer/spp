module;
#include <spp/macros.hpp>

export module spp.analyse.utils.case_utils;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct CasePatternVariantAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct LlvmCtx);

namespace spp::analyse::utils::case_utils {
  /// The codegen (stage 11) version of the core pattern
  /// analyser. It uses the same core function, but injects
  /// codegen steps into the analyser, producing IR, which
  /// is propagated out of the function.
  SPP_EXP_FUN auto CreateAndAnalysePatternEqFuncsLlvm(
    Vec<CasePatternVariantAst*> const &elems,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> Vec<llvm::Value*>;

  /// The comptime (stage 9) version of the core pattern
  /// analyser. It uses the same core function, but injects
  /// a comptime generation call, returning the obtained
  /// transformations.
  SPP_EXP_FUN auto CreateAndAnalysePatternEqCompTime(
    Vec<CasePatternVariantAst*> const &elems,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Vec<Unique<ExpressionAst>>;

  /// The core pattern analyser wrapper, which hooks into the
  /// internal shared pattern analyser. It handles literal/expr
  /// equality, destructure decomposition, comparison function
  /// call generation, etc.
  SPP_EXP_FUN auto CreateAndAnalysePatternEqFuncsDummyCore(
    Vec<CasePatternVariantAst*> const &elems,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void;

  /// Validation logic for ensuring the branches all return the
  /// same type, making type inference consistent and sound. A
  /// type-check between the terminating statements is performed.
  SPP_EXP_FUN auto ValidateInconsistentTypes(
    Vec<CaseExpressionBranchAst*> const &branches,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> Tup<Pair<Ast*, Shared<TypeAst>>, Vec<Pair<Ast*, Shared<TypeAst>>>>;

  /// Similar to the type check validation, but for memory. This
  /// is more complex as we must check every symbol, and reset
  /// its status to the original snapshots. The final symbol
  /// status is based off the consistent status's of every branch.
  /// Errors are not thrown if one branch moves a symbol and
  /// another doesn't; but the symbol mem-info is updated such
  /// that *using* that symbol later in the function would
  /// cause an error.
  SPP_EXP_FUN auto ValidateInconsistentMemory(
    Ast *parent,
    Vec<CaseExpressionBranchAst*> const &branches,
    VariableSymbol *subject,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void;
}
