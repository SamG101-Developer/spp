module;
#include <spp/macros.hpp>

export module spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_coros;
import spp.utils.ptr;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts::meta {
  SPP_EXP_CLS enum class CompilerStage : std::uint8_t;
  SPP_EXP_CLS struct MetaGuard;
}

SPP_EXP_CLS enum class spp::asts::meta::CompilerStage : std::uint8_t {
  kNone = 0,
  kGenTopLvlScopes,     // stage 2
  kGenTopLvlAliases,    // stage 3
  kQualifyTypes,        // stage 4
  kLoadSupScopes,       // stage 5
  kAttachSupScopes,     // the tail of stage 5
  kPreAnalyseSemantics, // stage 6
  kAnalyseSemantics,    // stage 7
  kCheckMemory,         // stage 8
  kCompTimeResolve,     // stage 9
  kMonomorphise,        // between stages 9 and 10
  kPreCodeGen,          // stage 10
  kCodeGen,             // stage 11
};

namespace spp::asts {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct LoopExpressionAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct LlvmLoopInfo;
  SPP_EXP_CLS struct CompilerMetaDataState;
  SPP_EXP_CLS struct CompilerMetaData;

  /**
   * Generic parameter names mapped to the types an object initializer infers their arguments from.
   */
  SPP_EXP_CLS
  using GenericInferenceBindings = Map<
    Shared<IdentifierAst>, Shared<TypeAst>,
    utils::ptr::ptr_hash<Shared<IdentifierAst>>, utils::ptr::ptr_eq<Shared<IdentifierAst>>>;
}

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmCtx;
}

/**
 * The llvm blocks belonging to a single enclosing loop, tracked so that @c exit and @c skip statements can branch to
 * the correct loop. The stack of these is ordered outermost-first, so the innermost loop is the back element.
 */
SPP_EXP_CLS struct spp::asts::meta::LlvmLoopInfo {
  llvm::BasicBlock *CondBB;
  llvm::BasicBlock *EndBB;
  llvm::PHINode *Phi;
  llvm::Value *EnteredFlag;

  /**
   * The scope the loop statement itself is written in - the first scope an @c exit or a @c skip is *not* leaving.
   * Every scope between the jump and this one is being left without reaching its end, so their drops are emitted at
   * the jump instead; this is where that walk stops.
   */
  analyse::scopes::Scope const *ScopeContainingLoop;
};

SPP_EXP_CLS struct spp::asts::meta::CompilerMetaDataState {
  CompilerStage CurrentStage;
  Shared<TypeAst> ReturnTypeOverloadResolverType;
  Shared<IdentifierAst> AssignmentTarget;
  Shared<TypeAst> AssignmentTargetType;
  bool IgnoreMissingElseBranchForInference;
  ExpressionAst *CaseCondition;

  /**
   * The symbols the surrounding @c "case ... of" expressions take when their patterns bind by move, while their
   * branches are being walked. The take is marked once, after the branches, because they have to bind off the value
   * first - but a @c ret or a loop jump inside a branch is checked before that happens, and would otherwise report a
   * subject as a value the branch abandoned when the @c case is exactly what consumed it.
   */
  Vec<Shared<IdentifierAst>> CaseConsumedSubjects;

  /**
   * The @c defer keyword whose expression is currently being analysed, or @c nullptr outside one. A deferred
   * expression runs because its scope is being left, so nothing inside it may leave that scope itself - and @c ?
   * expands to a @c ret, so it is caught here rather than by @c Terminates , which reports only unconditional exits.
   * Cleared when entering a closure body: a @c ? there returns from the closure, not from the deferring function.
   */
  TokenAst *WithinDeferTok = nullptr;
  analyse::scopes::TypeSymbol *ClsSym;
  analyse::scopes::Scope *OverriddenScopeForClosure;
  analyse::scopes::Scope *EnclosingFunctionScope;
  TokenAst *EnclosingFunctionFlavour;
  Vec<Shared<TypeAst>> EnclosingFunctionRetType;
  Vec<Shared<TypeAst>> EnclosingFunctionSourceRetType;
  TokenAst *EnclosingFunctionCmp;
  analyse::scopes::Scope *CurrentLambdaOuterScope;
  FunctionPrototypeAst *TargetCallFunctionPrototype;
  bool TargetCallWasFunctionAsync;
  bool PreventAutoGeneratorResume;
  Shared<TypeAst> LetStatementExplicitType;
  ExpressionAst *LetStatementValue;
  bool LetStatementFromUninitialized;

  /**
   * Set while a destructure's expanded bindings are checked. Each binding reads one field off the value, so each one
   * records a partial move of it, and the destructure marks the whole value moved once they are done. That makes the
   * expansion look identical to taking the value apart a piece at a time, which is the one thing a value with a
   * destructor may not have done to it - so the checks that refuse that have to know which of the two they are
   * looking at.
   */
  bool DestructuringValue;

  /**
   * When set, a local variable's initializer is this already-generated llvm value rather than the result of
   * code-generating @c LetStatementValue. Used to bind a function/closure parameter directly to its incoming
   * @c llvm::Argument, since there is no expression AST to codegen for it (see
   * @c FunctionParameterGroupAst::Stage11_CodeGen).
   */
  llvm::Value *LetStatementPrecomputedValue;

  std::size_t LoopCurrentDepth;
  LoopExpressionAst *LoopCurrentAst;
  Shared<Map<std::size_t, Tup<ExpressionAst*, Shared<TypeAst>, analyse::scopes::Scope*>>> LoopReturnTypes;
  Shared<TypeAst> ObjectInitType;
  /**
   * The object-initializer bindings a generic argument is inferred from, and the parameters they are inferred onto.
   *
   * @n
   * Held behind a @c Shared , like @c LoopReturnTypes , because @c Save copies every field it tracks and these two are
   * the only maps among them: a save that copied them element-wise would pay for a map of shared pointers on entry to
   * every guarded region, and almost no region touches them. The only writer replaces the whole map rather than
   * inserting into one, so sharing a map with a saved state can never let a region write through to it.
   */
  Shared<GenericInferenceBindings> InferSource;
  Shared<GenericInferenceBindings> InferTarget;
  ExpressionAst *PostfixExpressionLhs;

  ExpressionAst *UnaryExpressionRhs;
  bool SkipTypeAnalysisGenericChecks;
  analyse::scopes::Scope *TypeAnalysisTypeScope;
  Shared<TypeAst> IgnoreCmpGeneric;
  bool AllowMoveDeref;
  llvm::BasicBlock *LlvmEndBB;
  codegen::LlvmCtx *LlvmCtx;

  /**
   * Set when the consumer ast of an expression needs the address of the storage it names (an assignment target, or a
   * borrow being passed into a function), rather than its value. Only expressions that name storage use it.
   */
  bool LlvmWantAddress;

  llvm::Value *LlvmAssignmentTarget;
  llvm::Value *LlvmAssignmentTargetType;
  llvm::Value *LlvmCaseCondition;

  llvm::PHINode *LlvmPhi;
  Vec<LlvmLoopInfo> LlvmLoopStack;
  Map<
    Shared<IdentifierAst>, Unique<ExpressionAst>,
    utils::ptr::ptr_hash<Shared<IdentifierAst>>, utils::ptr::ptr_eq<Shared<IdentifierAst>>> CmpArgs; // Todo: struct
  Vec<TypeAst*> CmpGnTypeArgs;
  Vec<ExpressionAst*> CmpGnCompArgs;
  Unique<ExpressionAst> CmpResult;

  /**
   * Whether the comp-time frame currently being resolved has hit a @c ret . A @c ret nested inside a @c case branch
   * has to stop the statement loop that contains the @c case , or the statements after it go on resolving and
   * overwrite @c CmpResult with a value the call never reached. Like @c CmpResult it is deliberately not saved and
   * restored, so that it passes back up; the function implementation opens and closes a frame around it.
   */
  bool CmpReturned = false;

  bool IgnoreAccessModifierViolations;

  bool SkipSubstitutedConstraintChecks = false;

  /**
   * Whether the module being analysed is the entry point a test build generates. Only there may a unit test be called:
   * it is the one caller that is supposed to run them.
   */
  bool IsTestHarness = false;
  bool AllowAbstractType;

  /**
   * Whether a comp generic argument written as a plain name should resolve to what that name is bound to in the
   * current scope. Only set while re-analysing a generic function instantiation's own body, which is a private clone
   * of the template's, because the resolution rewrites the argument in place: every other analysis can be looking at
   * an ast shared with the template (an instantiated "sup" scope keeps the template's ast node), where baking one
   * instantiation's bindings in would corrupt the template for every other caller.
   */
  bool ResolveBoundCompGenerics;

  /**
   * The coroutine currently being generated into, for "gen" and "res" to reach. Shared rather than uniquely owned so
   * that @c CompilerMetaData::Save can copy it into the snapshot like every other field: a @c Unique could only be
   * moved, which left this null for the whole duration of the saved scope, and so nullptr for any "gen" nested inside
   * one - a "gen" in a case branch, a loop body, and so on.
   */
  Shared<codegen::LlvmGenerator> LlvmGenerator;

  /**
   * The coroutine's promise, held as the alloca it is rather than as a bare value, so that a "gen" reaching a slot
   * through it takes the struct type from the allocation itself. The two cannot then disagree about where the send
   * slot begins, which they would if each rebuilt the type from the coroutine's signature separately.
   */
  llvm::AllocaInst *LlvmGeneratorState;
};

/**
 * Shared metadata for ASTs, exclusive to the stage of compilation taking place. For example, tracking if an assignment
 * is taking place, when the RHS expression is being analysed. Use a pooled save/restore history. This is a hand-rolled
 * stack over a vector that never shrinks: `_Depth` is the live top-of-stack, and slots above it are parked (retaining
 * their allocated buffers) for reuse by the next Save. This avoids the alloc/free churn a `std::stack<..., std::deque>`
 * incurs across nested Save/Restore cycles.
 */
SPP_EXP_CLS struct spp::asts::meta::CompilerMetaData : CompilerMetaDataState {
private:
  Vec<CompilerMetaDataState> _History;
  std::size_t _Depth = 0;

public:
  CompilerMetaData();

  auto Save() -> void;

  auto Restore(bool heavy = false) -> void;

  SPP_ATTR_NODISCARD auto Depth() const -> std::size_t;
};

/**
 * Scoped @c CompilerMetaData::Save / @c Restore . The pair has to bracket exactly, and writing it by hand means a
 * @c return or a raised @c SemanticError between the two leaks the saved state into whatever runs next - which is
 * why @c DetermineOverload carries a loop that unwinds back to a remembered depth by hand. Declaring one of these
 * instead ties the restore to the scope, so both cases are handled by the language.
 */
SPP_EXP_CLS struct spp::asts::meta::MetaGuard {
  explicit MetaGuard(CompilerMetaData *meta, bool heavy = false);

  ~MetaGuard();

  MetaGuard(MetaGuard const &) = delete;

  auto operator=(MetaGuard const &) -> MetaGuard& = delete;

private:
  CompilerMetaData *_Meta;
  bool _Heavy;
};
