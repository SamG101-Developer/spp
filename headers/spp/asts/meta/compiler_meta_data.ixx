module;
#include <spp/macros.hpp>

export module spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import ankerl.unordered_dense;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaDataState;
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
    SPP_EXP_CLS struct LLvmCtx;
}

SPP_EXP_CLS struct spp::asts::meta::CompilerMetaDataState {
    double CurrentStage;
    TypeAst* ReturnTypeOverloadResolverType;
    IdentifierAst *AssignmentTarget;
    TypeAst *AssignmentTargetType;
    bool IgnoreMissingElseBranchForInference;
    ExpressionAst *CaseCondition;
    analyse::scopes::TypeSymbol *ClsSym;
    analyse::scopes::Scope *OverriddenScopeForClosure;
    analyse::scopes::Scope *EnclosingFunctionScope;
    TokenAst *EnclosingFunctionFlavour;
    Vec<Unique<TypeAst>> EnclosingFunctionRetType;
    Vec<TypeAst*> EnclosingFunctionSourceRetType;
    TokenAst *EnclosingFunctionCmp;
    analyse::scopes::Scope *CurrentLambdaOuterScope;
    FunctionPrototypeAst *TargetCallFunctionPrototype;
    bool TargetCallWasFunctionAsync;
    bool PreventAutoGeneratorResume;
    TypeAst* LetStatementExplicitType;
    ExpressionAst *LetStatementValue;
    bool LetStatementFromUninitialized;
    bool LoopDoubleCheckActive;
    std::size_t LoopCurrentDepth;
    LoopExpressionAst *LoopCurrentAst;
    ankerl::unordered_dense::map<std::size_t, std::tuple<ExpressionAst*, Unique<TypeAst>, analyse::scopes::Scope*>> LoopReturnTypes;
    TypeAst* ObjectInitType;
    ankerl::unordered_dense::map<IdentifierAst*, TypeAst*, utils::ptr::ptr_hash<IdentifierAst*>, utils::ptr::ptr_eq<IdentifierAst*>> InferSource;
    ankerl::unordered_dense::map<IdentifierAst*, TypeAst*, utils::ptr::ptr_hash<IdentifierAst*>, utils::ptr::ptr_eq<IdentifierAst*>> InferTarget;
    ExpressionAst *PostfixExpressionLhs;
    ExpressionAst *UnaryExpressionRhs;
    bool SkipTypeAnalysisGenericChecks;
    analyse::scopes::Scope *TypeAnalysisTypeScope;
    TypeAst* IgnoreCmpGeneric;
    bool AllowMoveDeref;
    llvm::BasicBlock *LlvmEndBB;
    codegen::LLvmCtx *LlvmCtx;
    llvm::Value *LlvmAssignmentTarget;
    llvm::Value *LlvmAssignmentTargetType;
    llvm::PHINode *LlvmPhi;
    ankerl::unordered_dense::map<IdentifierAst*, Unique<ExpressionAst>, utils::ptr::ptr_hash<IdentifierAst*>, utils::ptr::ptr_eq<IdentifierAst*>> CmpArgs;
    Unique<ExpressionAst> CmpResult;
    bool IgnoreAccessModifierViolations;
};

/**
 * Shared metadata for ASTs, exclusive to the stage of compilation taking place. For example, tracking if an assignment
 * is taking place, when the RHS expression is being analysed.
 */
SPP_EXP_CLS struct spp::asts::meta::CompilerMetaData : CompilerMetaDataState {
private:
    std::stack<CompilerMetaDataState> _History;

public:
    CompilerMetaData();

    auto Save() -> void;

    auto Restore(bool heavy = false) -> void;

    SPP_ATTR_NODISCARD auto Depth() const -> std::size_t;
};
