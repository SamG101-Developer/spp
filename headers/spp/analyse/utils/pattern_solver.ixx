module;
#include <spp/macros.hpp>

export module spp.analyse.utils.pattern_solver;
import spp.analyse.scopes.symbols;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::utils::pattern_solver, class PatternSolver);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::pattern_solver {
  /// One step from a value to a part of it: into the payload of the
  /// variant member it holds ("Payload", by the member's index), or
  /// into one of its fields ("Field", by its position in
  /// "type_members::GetAllParts").
  SPP_EXP_CLS struct PathStep {
    enum class Kind { Payload, Field } K = Kind::Field;

    std::size_t Index = 0;

    /// The field's name, or its index written out; none for a payload.
    Shared<IdentifierAst> Name;

    /// The type of the value the step is taken from: the variant, for
    /// a payload; the type (or the variant's member) holding the
    /// field, for a field.
    TypeRef Of;

    /// Whether the field holds a borrow, which the step reads through.
    bool Through = false;
  };

  /// What a "case" matches, from its patterns and its subject's type:
  /// every stage reads this one answer - typing (whether an "else" is
  /// needed), codegen (whether the fall-through is reachable), and the
  /// memory check (which paths there are).
  SPP_EXP_CLS struct CaseCoverage {
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

  /// What a "case" does with its subject on each way through it: on
  /// each branch's match edge, and on the edge that falls through
  /// every branch, if there is one. A branch binding by move off it
  /// takes it. A branch narrowing it to members that are all "Copy"
  /// ("is None"), and an "else" or fall-through left only such
  /// members, find it owes nothing. Anything else keeps it owned on
  /// that path.
  SPP_EXP_CLS struct SubjectEdges {
    Vec<SubjectOnMatch> OnMatch;
    SubjectOnMatch OnFallThrough = SubjectOnMatch::kKeep;
  };

  /// One thing a pattern tests of the value at "Path": which member a
  /// variant holds ("Tag": one of "Members", as tags of "Variant"), or a
  /// literal ("Literal": the call "<value>.eq(&<literal>)", built and
  /// analysed once, where the value is a hidden borrow ("Value") that
  /// codegen points at "Path" in the subject, and the comptime pass
  /// gives the comptime value there).
  SPP_EXP_CLS struct PatternCheck {
    enum class Kind { Tag, Literal } K = Kind::Tag;

    Vec<PathStep> Path;

    TypeRef Variant;

    Vec<std::size_t> Members;

    /// The members' types, for "Tag" (read at compile time).
    Vec<TypeRef> MemberRefs;

    Shared<VariableSymbol> Value;

    Unique<ExpressionAst> Call;
  };

  /// What one branch tests: per pattern, its checks in the order they
  /// are made (a variant's tag before anything inside its payload), all
  /// of which have to hold.
  SPP_EXP_CLS struct BranchPlan {
    Vec<Vec<PatternCheck>> Checks;
  };

  /// The one answer a "PatternSolver" gives: what each branch's
  /// patterns check, what the branches cover, and what each way through
  /// the "case" does with its subject.
  SPP_EXP_CLS struct MatchPlan {
    /// Per branch.
    Vec<BranchPlan> Branches;

    /// The hidden borrow of the subject the checks read from
    /// ("DeclareSubjectValue").
    Shared<VariableSymbol> Subject;

    /// The subject's type as the condition gave it, before any branch
    /// narrowed it: whether it is a borrow says whether its generated
    /// value is the subject's address.
    TypeRef SubjectHeld;

    /// Whether anything reads "Subject": a check, or a comparing branch.
    bool ReadsSubject = false;

    CaseCoverage Coverage;

    SubjectEdges Edges;
  };

  /// The name of the hidden borrow of a "case" subject: the "$" cannot
  /// be written, so no binding can collide with it, and a "case" nested
  /// in a branch declares its own in its own scope.
  SPP_EXP_FUN auto SubjectValueName() -> Str;

  /// Declare the hidden borrow of a subject of type "subject"
  /// in the current scope (the "case"'s): a comparing branch
  /// ("== x") tests it rather than the condition again, which
  /// would evaluate it again. Doesn't build a "let" statement,
  /// just a representing symbol.
  SPP_EXP_FUN auto DeclareSubjectValue(
    TypeRef const &subject, std::size_t pos,
    ScopeManager &sm) -> Shared<VariableSymbol>;

  /// Whether a branch's patterns are an "else" ("else" or "else
  /// case"), which matches whatever reaches it.
  SPP_EXP_FUN auto IsElseBranch(
    CaseExpressionBranchAst const &branch) -> bool;
}

/// Solves the patterns of one "case": built from its branches and its
/// subject's type, it reads each pattern once into nodes, each at the
/// position it tests, and works out what the branches cover from them.
/// An unguarded branch matches outright: everything, for an "else" or
/// a pattern that cannot fail on the subject's own type; the members
/// it selects, for patterns that cannot fail within them; and "true"
/// or "false" of a "Bool" for "== true" / "== false". This holds
/// through every position a pattern tests (a tuple's elements, an
/// attribute's pattern), so a branch matching only what earlier ones
/// match is unreachable, and once every value is matched there is no
/// fall-through.
SPP_EXP_CLS class spp::analyse::utils::pattern_solver::PatternSolver {
public:
  PatternSolver(
    Vec<CaseExpressionBranchAst*> branches, TypeRef const &subject,
    Shared<VariableSymbol> subject_value, Scope const &scope);

  PatternSolver(PatternSolver const &) = delete;
  ~PatternSolver();
  auto operator=(PatternSolver const &) -> PatternSolver& = delete;

  /// The plan: the nodes; the checks, with the literal ones analysed
  /// where "sm" stands (the "case"'s scope, where their hidden values
  /// are declared); then the coverage worked out over the nodes.
  auto Solve(
    ScopeManager &sm, CompilerMetaData &meta) const -> MatchPlan;

private:
  /// All the branches on the case being solved.
  Vec<CaseExpressionBranchAst*> _Branches;

  /// The case's subject type, inferred from the case's condition.
  TypeRef _Subject;

  /// The variable symbol created for the condition.
  Shared<VariableSymbol> _SubjectValue;

  /// The scope for the case's expression.
  Scope const *_Scope;
};
