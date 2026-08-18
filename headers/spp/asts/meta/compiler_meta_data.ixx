module;
#include <spp/macros.hpp>

export module spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_coros;
import spp.utils.ptr;
import spp.utils.types;
import llvm;
import std;

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
};

SPP_EXP_CLS struct spp::asts::meta::CompilerMetaDataState {
  double CurrentStage;
  Shared<TypeAst> ReturnTypeOverloadResolverType;
  Shared<IdentifierAst> AssignmentTarget;
  Shared<TypeAst> AssignmentTargetType;
  bool IgnoreMissingElseBranchForInference;
  ExpressionAst *CaseCondition;
  analyse::scopes::TypeSymbol *ClsSym;
  analyse::scopes::Scope *OverriddenScopeForClosure;
  analyse::scopes::Scope *EnclosingFunctionScope;
  TokenAst *EnclosingFunctionFlavour;
  SharedVec<TypeAst> EnclosingFunctionRetType;
  SharedVec<TypeAst> EnclosingFunctionSourceRetType;
  TokenAst *EnclosingFunctionCmp;
  analyse::scopes::Scope *CurrentLambdaOuterScope;
  FunctionPrototypeAst *TargetCallFunctionPrototype;
  bool TargetCallWasFunctionAsync;
  bool PreventAutoGeneratorResume;
  Shared<TypeAst> LetStatementExplicitType;
  ExpressionAst *LetStatementValue;
  bool LetStatementFromUninitialized;

  /**
   * When set, a local variable's initializer is this already-generated llvm value rather than the result of
   * code-generating @c LetStatementValue. Used to bind a function/closure parameter directly to its incoming
   * @c llvm::Argument, since there is no expression AST to codegen for it (see
   * @c FunctionParameterGroupAst::Stage11_CodeGen).
   */
  llvm::Value *LetStatementPrecomputedValue;

  bool LoopDoubleCheckActive;
  std::size_t LoopCurrentDepth;
  LoopExpressionAst *LoopCurrentAst;
  Shared<Map<std::size_t, Tup<ExpressionAst*, Shared<TypeAst>, analyse::scopes::Scope*>>> LoopReturnTypes;
  Shared<TypeAst> ObjectInitType;
  Map<
    Shared<IdentifierAst>, Shared<TypeAst>,
    utils::ptr::ptr_hash<Shared<IdentifierAst>>, utils::ptr::ptr_eq<Shared<IdentifierAst>>> InferSource; // Todo: struct
  Map<
    Shared<IdentifierAst>, Shared<TypeAst>,
    utils::ptr::ptr_hash<Shared<IdentifierAst>>, utils::ptr::ptr_eq<Shared<IdentifierAst>>> InferTarget; // Todo: struct
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
  bool IgnoreAccessModifierViolations;
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
  llvm::Value *LlvmGeneratorState;
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
