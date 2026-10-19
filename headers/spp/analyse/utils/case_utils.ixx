module;
#include <spp/macros.hpp>

export module spp.analyse.utils.case_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct CasePatternVariantAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct LlvmCtx);

namespace spp::analyse::utils::case_utils {
  /// What a "case" matches ("CoverageOf"), from its patterns and its
  /// subject's type: every stage reads this one answer - typing
  /// (whether an "else" is needed), codegen (whether the fall-through
  /// is reachable), and the memory check (which paths there are).
  SPP_EXP_CLS struct CaseCoverage {
    /// The subject's variant members, none for any other subject.
    Vec<TypeRef> VariantMembers;

    /// The members no earlier unguarded branch matched outright, as
    /// each branch is reached.
    Vec<Vec<TypeRef>> RemainingBefore;

    /// Whether each branch can match at all.
    Vec<bool> Reachable;

    /// For each branch that cannot, the earliest branch by which
    /// everything it matches is matched.
    Vec<Opt<std::size_t>> ShadowedBy;

    /// For each branch, the member of the subject each of its patterns
    /// selects: one naming a member, or refining one ("Some[Some[T]]"
    /// for "Some[Opt[T]]"), selects it; anything else (a sub-variant,
    /// the subject's own type, a test) selects none.
    Vec<Vec<Opt<TypeRef>>> SelectedMembers;

    /// Whether control can fall through every branch, and the members
    /// it is left holding if so.
    bool FallsThrough = true;

    Vec<TypeRef> Remaining;
  };

  /// Whether a branch's patterns are an "else" ("else" or "else
  /// case"), which matches whatever reaches it.
  SPP_EXP_FUN auto IsElseBranch(
    CaseExpressionBranchAst const &branch) -> bool;

  /// What "branches" match of a subject of type "subject". An
  /// unguarded branch matches outright: everything, for an "else" or
  /// a pattern that cannot fail on the subject's own type; the members
  /// it selects, for patterns that cannot fail within them; and "true"
  /// or "false" of a "Bool" for "== true" / "== false". This holds
  /// through every position a pattern tests (a tuple's elements, an
  /// attribute's pattern), so a branch matching only what earlier ones
  /// match is unreachable, and once every value is matched there is no
  /// fall-through.
  SPP_EXP_FUN auto CoverageOf(
    Vec<CaseExpressionBranchAst*> const &branches, TypeRef const &subject,
    Scope const &scope) -> CaseCoverage;

  /// What a "case" does with its subject on each way through it
  /// ("PlanSubjectEdges"): on each branch's match edge, and on the
  /// edge that falls through every branch, if there is one.
  SPP_EXP_CLS struct SubjectEdges {
    Vec<SubjectOnMatch> OnMatch;
    SubjectOnMatch OnFallThrough = SubjectOnMatch::kKeep;
  };

  /// Plan the subject's edges through "branches", given what they
  /// cover. A branch binding by move off it takes it. A branch
  /// narrowing it to members that are all "Copy" ("is None"), and an
  /// "else" or fall-through left only such members, find it owes
  /// nothing. Anything else keeps it owned on that path.
  SPP_EXP_FUN auto PlanSubjectEdges(
    Vec<CaseExpressionBranchAst*> const &branches, CaseCoverage const &coverage) -> SubjectEdges;

  /// The match edge of "branch", once its patterns and guard have
  /// matched: what "meta.CaseSubjectOnMatch" says is done to
  /// "meta.CaseSubject" (taken whole, or the part the condition
  /// names; found to owe nothing; or kept). What is taken has to
  /// have every owned part bound by the patterns first.
  SPP_EXP_FUN auto ApplySubjectOnMatch(
    CaseExpressionBranchAst const &branch, ScopeManager &sm,
    CompilerMetaData const &meta) -> void;

  /// The codegen (stage 11) version of the core pattern
  /// analyser. It uses the same core function, but injects
  /// codegen steps into the analyser, producing IR, which
  /// is propagated out of the function.
  SPP_EXP_FUN auto CreateAndAnalysePatternEqFnsLlvm(
    Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
    CompilerMetaData *meta, LlvmCtx *ctx) -> Vec<llvm::Value*>;

  /// The comptime (stage 9) version of the core pattern
  /// analyser. It uses the same core function, but injects
  /// a comptime generation call, returning the obtained
  /// transformations.
  SPP_EXP_FUN auto CreateAndAnalysePatternEqCompTime(
    Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
    CompilerMetaData *meta) -> Vec<Unique<ExpressionAst>>;

  /// The core pattern analyser wrapper, which hooks into the
  /// internal shared pattern analyser. It handles literal/expr
  /// equality, destructure decomposition, comparison function
  /// call generation, etc.
  SPP_EXP_FUN auto CreateAndAnalysePatternEqFnsDummyCore(
    Vec<CasePatternVariantAst*> const &elems,
    ScopeManager *sm, CompilerMetaData *meta) -> void;

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
