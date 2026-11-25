module;
#include <spp/macros.hpp>

export module spp.analyse.utils.case_utils;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.type_join;
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
use(spp::codegen, struct LlvmCtx);

use_alias(spp::asts::meta, CompilerMetaData);

namespace spp::analyse::utils::case_utils {
  /// A branch's match edge, once its patterns and guard have
  /// matched: what "meta.CaseSubjectOnMatch" says is done to
  /// "meta.CaseSubject" (taken whole, or the part the condition
  /// names; or kept). Patterns leaving an owned part unbound take
  /// only what they bind, so the subject's owner drops the rest.
  SPP_EXP_FUN auto ApplySubjectOnMatch(
    ScopeManager &sm, CompilerMetaData const &meta) -> void;

  /// Point the hidden borrow of a "case" subject (the plan's
  /// "Subject") at the subject's generated value "llvm_subject": a
  /// borrowed subject ("SubjectHeld") is an address already, and any
  /// other is stored in a slot of its own.
  SPP_EXP_FUN auto BindSubjectValue(
    MatchPlan const &plan, llvm::Value *llvm_subject,
    LlvmCtx *ctx) -> llvm::Value*;

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

  /// The branches' values, given to the join their type is agreed
  /// by ("type_join::TypeJoin"): each branch's final member and
  /// type, read here, onto what the case is assigned to, if it is.
  SPP_EXP_FUN auto JoinBranches(
    Vec<CaseExpressionBranchAst*> const &branches,
    ScopeManager &sm, CompilerMetaData *meta)
    -> type_join::TypeJoin;

  /// Check each branch's memory from the state before the "case"
  /// (carrying a guard's moves past its branch), then join the
  /// paths out into the state after it: what the paths disagree
  /// about is no error until it is used ("maybe" states). A value
  /// of an enclosing scope held on only some paths is dropped on
  /// each path still holding it: each branch is given its own
  /// ("drop_utils::DropEdge::kEnd"), and what the path falling through every
  /// branch drops is returned. The temporaries the condition made
  /// ("condition_temporaries", "drop_utils::TemporaryRegion") are
  /// held on every path out, each dropping what it still holds, but
  /// those held only where a boolean condition is true
  /// ("held_in_first_branch") are held in its first branch alone.
  SPP_EXP_FUN auto ValidateInconsistentMemory(
    Ast *parent, Vec<CaseExpressionBranchAst*> const &branches,
    CaseCoverage const &coverage, SubjectEdges const *subject_edges,
    Vec<VariableSymbol*> const &condition_temporaries, Vec<VariableSymbol*> const &held_in_first_branch,
    ScopeManager *sm, CompilerMetaData *meta) -> Vec<drop_utils::OwnedValue>;
}
