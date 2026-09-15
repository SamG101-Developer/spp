module;
#include <spp/macros.hpp>

export module spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_coros;
import spp.utils.ptr;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LoopExpressionAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts::meta, enum class CompilerStage : std::uint8_t);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::asts::meta, struct CompilerMetaDataState);
use(spp::asts::meta, struct LlvmLoopInfo);
use(spp::asts::meta, struct MetaGuard);
use(spp::codegen, struct LlvmCtx);

SPP_EXP_CLS enum class spp::asts::meta::CompilerStage : std::uint8_t {
  kNone = 0,
  kGenTopLvlScopes, // stage 2
  kGenTopLvlAliases, // stage 3
  kQualifyTypes, // stage 4
  kLoadSupScopes, // stage 5
  kAttachSupScopes, // the tail of stage 5
  kPreAnalyseSemantics, // stage 6
  kAnalyseSemantics, // stage 7
  kCheckMemory, // stage 8
  kCompTimeResolve, // stage 9
  kMonomorphise, // between stages 9 and 10
  kPreCodeGen, // stage 10
  kCodeGen, // stage 11
};

namespace spp::asts::meta {
  /// Generic parameter names mapped to the types an object
  /// initializer infers their arguments from.
  SPP_EXP_CLS using GenericInferenceBindings = Map<
    Shared<IdentifierAst>, Shared<TypeAst>,
    utils::ptr::ptr_hash<Shared<IdentifierAst>>,
    utils::ptr::ptr_eq<Shared<IdentifierAst>>>;
}

/// The LLVM blocks belonging to a single enclosing loop,
/// tracked so that "exit" and "skip" loop flow control
/// statements can branch to the correct loop. The stack
/// of these is ordered outermost-first, so the innermost
/// loop is the back element.
SPP_EXP_CLS struct spp::asts::meta::LlvmLoopInfo {
  /// The LLVM building block for the condition.
  llvm::BasicBlock *CondBB = nullptr;

  /// The LLVM building block for the end section; the part
  /// after the loop.
  llvm::BasicBlock *EndBB = nullptr;

  /// The LLVM phi node to receive values into from the loop
  /// exiting with a value.
  llvm::PHINode *Phi = nullptr;

  /// Whether the loop has run its body at least once or not,
  /// so we know whether to take the "else" block under the
  /// loop or not.
  llvm::Value *EnteredFlag = nullptr;

  /// The scope that a loop statement is written in; were we
  /// to break out of this loop, this is the scope we'd end
  /// up in. All the scopes between the jump, and this one,
  /// are being left without reaching the end, so their drops
  /// need to be emitted here instead.
  Scope const *ScopeContainingLoop = nullptr;
};

/// The master meta context used for additional information
/// passing between asts.
SPP_EXP_CLS struct spp::asts::meta::CompilerMetaDataState {
  /// The current stage. This is tracked here for two reasons;
  /// one, helper functions have no notion of the stage they
  /// are called from. and two, some stage4 functions call a
  /// stage 7 ont-hit analysis, so there is some stage-dependent
  /// behaviour that might need to be enforced.
  CompilerStage CurrentStage;

  /// When a function is being called with two overloads that
  /// differ only in their return type, we can provide a type
  /// that resolves this, maybe from a "let: Type = ...", or a
  /// "cmp: Type = ..." explicit designator.
  Shared<TypeAst> ReturnTypeOverloadResolverType;

  /// The target of an assignment. This provides a handle that
  /// the rhs expression might want to use for example. Also
  /// used by "let" which is an assignment of sorts too.
  Shared<IdentifierAst> AssignmentTarget;

  /// Tracked with the assignment target, this is the type of
  /// the assignment target, always valid for "=" as the left
  /// side symbol already exists, where-as the "let" only
  /// sometimes has its type.
  Shared<TypeAst> AssignmentTargetType;

  /// There are some contexts where we allow ignoring a missing
  /// else branch, such as some basic type analysis that just
  /// needs to know the branch's returning type.
  bool IgnoreMissingElseBranchForInference;

  /// The condition on a "case" ast, being propagated into the
  /// pattern analysis, where it might get combined into a
  /// method from the operator, like "==" creating ".eq()" on the
  /// condition.
  ExpressionAst *CaseCondition;

  /// If we are consuming the case condition or not. It is stored
  /// as a vector so that nested "case" statements work properly,
  /// moving back to the correct case expression per case block.
  Vec<Shared<IdentifierAst>> CaseConsumedSubjects;

  /// Whether we are currently operating within a "defer"
  /// statement's expression - different rules for analysis and
  /// terminating.
  TokenAst *WithinDeferTok = nullptr;

  /// The actual "current scope" of the program, which has been
  /// hidden by the isolated closure scope being set to the
  /// current scope.
  Scope *OverriddenScopeForClosure;

  /// The function scope containing the surrounding function. This
  /// is not reset on a restore, so persists throughout all save/
  /// restore operations on "meta" during analysis.
  Scope *EnclosingFunctionScope;

  /// The function variant of the surrounding function: whether
  /// we are inside a subroutine (fun) or coroutine (cor). Needed
  /// for "ret" and "gen" position checking.
  TokenAst *EnclosingFunctionFlavour;

  /// The return type of the enclosing function type. Again needed
  /// for "ret" and "gen" type checking.
  Vec<Shared<TypeAst>> EnclosingFunctionRetType;

  /// The "source" return type of the enclosing function type.
  /// Needed for "ret" and "gen" type checking error reporting.
  Vec<Shared<TypeAst>> EnclosingFunctionSourceRetType;

  /// Whether the enclosing function is a "cmp" compile time
  /// function or not. Required because "cmp" functions cannot
  /// call non-"cmp" functions in their body.
  TokenAst *EnclosingFunctionCmp;

  /// The current "outer" closure scope. This is needed so that
  /// when we are in the "inner" closure scope, we can lookup
  /// symbols from the outer scope, and then move then in.
  Scope *CurrentLambdaOuterScope;

  /// The function prototype being called by a postfix function
  /// call operator. Needed for coroutine target checking during
  /// analysis, especially memory rules.
  FunctionPrototypeAst *TargetCallFunctionPrototype;

  /// Similar to above, but rather than tracking the variation
  /// of the target function prototype, check the calling
  /// convention for "async", for memory rules.
  bool TargetCallWasFunctionAsync;

  /// The explicit type, if provided, on a "let" statement,
  /// carried forward for local variable asts to analyse values
  /// against.
  Shared<TypeAst> LetStatementExplicitType;

  /// The value on a let statement, carried forward for local
  /// variables to manage destructuring for.
  ExpressionAst *LetStatementValue;

  /// Whether the "let" statement was for the uninitialized
  /// "let x: Type", so local variables don't try to get a value
  /// that's not there.
  bool LetStatementFromUninitialized;

  /// The precomputed LLVM version of the "LetStatementValue",
  /// so we don't have to regenerate the "let" value, which
  /// could cause scope-desync.
  llvm::Value *LetStatementPrecomputedValue;

  /// Loop depth tracking for "skip" and "exit" statements to
  /// use.
  std::size_t LoopCurrentDepth;

  /// Loop ast tracking through multiple depths, for error
  /// reporting with incompatible "exit" levels.
  LoopExpressionAst *LoopCurrentAst;

  /// Loop return type tracking over multiple levels, ensuring
  /// good type checking against "exit" statements at any
  /// level, targeting any level.
  Shared<Map<std::size_t, Tup<ExpressionAst*, Shared<TypeAst>, Scope*>>> LoopReturnTypes;

  /// The object initializer type, because the object initializer
  /// group needs it for generic inference.
  Shared<TypeAst> ObjectInitType;

  /// Critical to advanced generic inference, the infer source
  /// is the map of "arguments" such as function or object init
  /// arguments. These are compared by name against the inference
  /// targets to infer generics.
  Shared<GenericInferenceBindings> InferSource;

  /// Critical to advanced generic inference, the infer target
  /// is the map of "parameters" such as function param or object
  /// init class fields. These are compared by name against the
  /// inference sources to infer generics.
  Shared<GenericInferenceBindings> InferTarget;

  /// Track the left-hand-side of a postfix expression so that
  /// the operator being applied to it can read from it.
  ExpressionAst *PostfixExpressionLhs;

  /// Track the right-hand-side of a unary expression so that
  /// the operator being applied to it can reach from it.
  ExpressionAst *UnaryExpressionRhs;

  /// There are some instances where we want to analyse a type
  /// but not the generics attached to it, so allow that.
  /// Todo: Remove and use ->WithoutGenerics()->Stage7...()?
  bool SkipTypeAnalysisGenericChecks;

  /// The overriding type scope to analyse a type in. This is
  /// used for example from a type unary expression to provide
  /// the namespace scope for the type identifier ast.
  Scope *TypeAnalysisTypeScope;

  /// The comp generic parameter whose own type is being
  /// qualified, so generic instantiations made while doing
  /// so don't carry in its not-yet-typed symbol.
  Shared<IdentifierAst> IgnoreCmpGeneric;

  /// There are some instances where "moving" the value under
  /// a deref is allowed, because a move doesn't actually happen,
  /// but it appears to, for example with "a@ = 1".
  bool AllowMoveDeref;

  /// The "end" building block for "case" and "loop" blocks,
  /// so we have a way to branch to the end from case branches
  /// etc.
  llvm::BasicBlock *LlvmEndBB;

  /// The LLVM meta context containing the LLVM context, module,
  /// builder, etc. Overarching, persistent, LLVM context fields.

  /// Set when a consumer ast of an expression needs the address
  /// of the storage it is naming, like an assignment target or
  /// borrow argument, rather than the value. Only expressions
  /// naming storage need it.
  bool LlvmWantAddress;

  /// Stage 11 versions of the assignment target fields, needed
  /// for detecting handles etc.
  llvm::Value *LlvmAssignmentTarget;

  /// Stage 11 equivalent of the case condition above, needed
  /// for the case branches and patterns to interact with the
  /// condition, forming expressions and destructures etc.
  llvm::Value *LlvmCaseCondition;

  /// The phi node is needed so that inner branches can send
  /// data back into the case/loop owned phi node.
  llvm::PHINode *LlvmPhi;

  /// Loop tracking information during stage 11, for nested
  /// loops and control flow.
  Vec<LlvmLoopInfo> LlvmLoopStack;

  /// A collection of "cmp" compile-time information, typically
  /// used in stage 9, to compute instructions at compile time.
  Map<
    Shared<IdentifierAst>, Unique<ExpressionAst>,
    utils::ptr::ptr_hash<Shared<IdentifierAst>>,
    utils::ptr::ptr_eq<Shared<IdentifierAst>>> CmpArgs; // Todo: struct
  Vec<TypeAst*> CmpGnTypeArgs;
  Vec<ExpressionAst*> CmpGnCompArgs;
  Unique<ExpressionAst> CmpResult;
  bool CmpReturned = false;

  /// The outermost call a comp-time evaluation started from, and
  /// the scope it was written in. An error raised while a nested
  /// call is evaluated (std's arithmetic, an intrinsic) reports
  /// here, where the user wrote the expression.
  Ast const *CmpCallSite = nullptr;
  Scope *CmpCallSiteScope = nullptr;

  /// Ignore access modifier violations during analysis. This is
  /// for when certain asts map to functions private on STD types,
  /// so they can't manually be called, only mapped onto.
  bool IgnoreAccessModifierViolations;

  /// A monomorphization helper, to prevent constraints being
  /// checked for a generic-substituted type.
  /// Todo: Why is this needed (it IS needed).
  bool SkipSubstitutedConstraintChecks = false;

  /// Track whether we are in a S++ test harness ie is the module
  /// in the "tst" folder not "src". Used to prevent certain
  /// actions.
  bool IsTestHarness = false;

  /// Whitelist contexts where abstract types are allows to appear,
  /// such as for generic constraints. Abstract types are very
  /// limited in where they can be used.
  bool AllowAbstractType;

  /// The coroutine being generated into, for "gen" and ".res()"
  /// to interact with in stage 11.
  Shared<codegen::LlvmGenerator> LlvmGenerator;

  /// Additional coroutine information for the "gen" expression
  /// to interact with.
  llvm::AllocaInst *LlvmGeneratorState;
};

/// The CompilerMetaData inherits the state, which acts as
/// the "current version" of the meta context, and contains
/// save and restore capabilities to map properties back.
/// Hand-rolled pool system to prevent hot paths allocation
/// churn on save/restore.
SPP_EXP_CLS struct spp::asts::meta::CompilerMetaData :
  CompilerMetaDataState {
private:
  /// The history of the meta changes, snapshotted by the
  /// "Save" method.
  Vec<CompilerMetaDataState> _History;

  /// The tracking depth of the current version in the
  /// history.
  std::size_t _Depth = 0;

public:
  CompilerMetaData();

  /// Snapshot all the current values into the history, making
  /// them "restorable".
  auto Save() -> void;

  /// Restore all the light values (everything except function
  /// context, which we want to persist upwards). Set "heavy"
  /// to true to clear those too.
  auto Restore(bool heavy = false) -> void;

  /// Getter for the internal depth. This is used when an catchable
  /// error might have raised in between a "Save" and "Restore",
  /// and we want to manually sync the meta context depth (unlikely
  /// with the new guard though).
  SPP_ATTR_NODISCARD auto Depth() const -> std::size_t;
};

/// A scoped guard to prevent the desync of "Save"/"Restore"
/// when a catchable error raises between the two. Given the
/// scope exits at the error, we can call "Restore" there,
/// re-syncing the differences.
SPP_EXP_CLS struct spp::asts::meta::MetaGuard {
  /// Propagate the "heavy" tag into the "Restore" call for
  /// consistency.
  explicit MetaGuard(CompilerMetaData *meta, bool heavy = false);

  /// Destructor restores the context with the stored "heavy"
  /// tag.
  ~MetaGuard();

  /// No copying or assignment.
  MetaGuard(MetaGuard const &) = delete;
  auto operator=(MetaGuard const &) -> MetaGuard& = delete;

private:
  /// The meta context to restore to.
  meta::CompilerMetaData *_Meta;

  /// Whether to restore the "heavy" fields too.
  bool _Heavy;
};
