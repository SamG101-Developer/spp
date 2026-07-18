module;
#include <spp/macros.hpp>

export module spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import ankerl;
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
    bool LoopDoubleCheckActive;
    std::size_t LoopCurrentDepth;
    LoopExpressionAst *LoopCurrentAst;
    Shared<ankerl::unordered_dense::map<std::size_t, std::tuple<ExpressionAst*, Shared<TypeAst>, analyse::scopes::Scope*>>> LoopReturnTypes;
    Shared<TypeAst> ObjectInitType;
    ankerl::unordered_dense::map<Shared<IdentifierAst>, Shared<TypeAst>, utils::ptr::ptr_hash<Shared<IdentifierAst>>, utils::ptr::ptr_eq<Shared<IdentifierAst>>> InferSource;
    ankerl::unordered_dense::map<Shared<IdentifierAst>, Shared<TypeAst>, utils::ptr::ptr_hash<Shared<IdentifierAst>>, utils::ptr::ptr_eq<Shared<IdentifierAst>>> InferTarget;
    ExpressionAst *PostfixExpressionLhs;
    ExpressionAst *UnaryExpressionRhs;
    bool SkipTypeAnalysisGenericChecks;
    analyse::scopes::Scope *TypeAnalysisTypeScope;
    Shared<TypeAst> IgnoreCmpGeneric;
    bool AllowMoveDeref;
    llvm::BasicBlock *LlvmEndBB;
    codegen::LLvmCtx *LlvmCtx;
    llvm::Value *LlvmAssignmentTarget;
    llvm::Value *LlvmAssignmentTargetType;
    llvm::PHINode *LlvmPhi;
    ankerl::unordered_dense::map<Shared<IdentifierAst>, Unique<ExpressionAst>, utils::ptr::ptr_hash<Shared<IdentifierAst>>, utils::ptr::ptr_eq<Shared<IdentifierAst>>> CmpArgs;
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
