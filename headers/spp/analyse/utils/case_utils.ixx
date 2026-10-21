module;
#include <spp/macros.hpp>

export module spp.analyse.utils.case_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::utils::pattern_solver, struct CaseCoverage);
use(spp::analyse::utils::pattern_solver, struct MatchPlan);
use(spp::analyse::utils::pattern_solver, struct PatternCheck);
use(spp::analyse::utils::pattern_solver, struct SubjectEdges);
use(spp::asts, struct Ast);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct LlvmCtx);

namespace spp::analyse::utils::case_utils {
  /// The match edge of "branch", once its patterns and guard have
  /// matched: what "meta.CaseSubjectOnMatch" says is done to
  /// "meta.CaseSubject" (taken whole, or the part the condition
  /// names; found to owe nothing; or kept). What is taken has to
  /// have every owned part bound by the patterns first.
  SPP_EXP_FUN auto ApplySubjectOnMatch(
    CaseExpressionBranchAst const &branch, ScopeManager &sm,
    CompilerMetaData const &meta) -> void;

  /// Point the hidden borrow of a "case" subject (the plan's
  /// "Subject") at the subject's generated value "llvm_subject": a
  /// borrowed subject ("SubjectHeld") is an address already, and any
  /// other is stored in a slot of its own.
  SPP_EXP_FUN auto BindSubjectValue(
    MatchPlan const &plan, llvm::Value *llvm_subject,
    LlvmCtx *ctx) -> void;

  /// Generate "checks" (one pattern's, from the plan) against the
  /// subject its hidden borrow points at, in order, each only once the
  /// ones before it hold: "true" when every one holds, and when there
  /// are none.
  SPP_EXP_FUN auto EmitPatternChecks(
    MatchPlan const &plan, Vec<PatternCheck> const &checks,
    ScopeManager *sm, CompilerMetaData *meta,
    LlvmCtx *ctx) -> llvm::Value*;

  /// Give the plan's hidden borrows the subject's comptime value
  /// "value", each at its own path in it.
  SPP_EXP_FUN auto BindSubjectCompTimeValue(
    MatchPlan const &plan, Ast const &value) -> void;

  /// Whether every one of "checks" (one pattern's) holds at compile
  /// time, given "BindSubjectCompTimeValue": a tag by the type of the
  /// object there, a literal by its "eq" call.
  SPP_EXP_FUN auto ResolvePatternChecks(
    MatchPlan const &plan, Vec<PatternCheck> const &checks,
    ScopeManager *sm, CompilerMetaData *meta) -> bool;

  /// Validation logic for ensuring the branches all return the
  /// same type, making type inference consistent and sound. A
  /// type-check between the terminating statements is performed.
  SPP_EXP_FUN auto ValidateInconsistentTypes(
    Vec<CaseExpressionBranchAst*> const &branches,
    ScopeManager &sm, CompilerMetaData *meta)
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
    Ast *parent, Vec<CaseExpressionBranchAst*> const &branches,
    CaseCoverage const &coverage, VariableSymbol *subject,
    SubjectEdges const *subject_edges, ScopeManager *sm,
    CompilerMetaData *meta) -> void;
}
