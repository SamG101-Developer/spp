module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_branch_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct PatternGuardAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The @c CaseExpressionBranchAst represents a branch on a @c case block. It contains the patterns to match the @c case
 * expression against, can be "guarded", and contains the body of the block.
 */
SPP_EXP_CLS struct spp::asts::CaseExpressionBranchAst final : Ast, mixins::TypeInferrableAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optional comparison operator. This is for cases pattern matching cases that look something like
     * @code== 123 { ... }@endcode.
     */
    Unique<TokenAst> Op;

    /**
     * The list of patterns that this branch matches against. There can be more than 1 pattern for non-destructuring
     * operations.
     */
    UniqueVec<CasePatternVariantAst> Patterns;

    /**
     * The optional guard for the case branch. This is a boolean expression that must evaluate to true for destructuring
     * patterns only.
     */
    Unique<PatternGuardAst> Guard;

    /**
     * The body of the case branch. This is an inner scope that contains the statements that will be executed if the
     * branch matches.
     */
    Unique<InnerScopeExpressionAst> Body;

    /**
     * Construct the CaseExpressionBranchAst with the arguments matching the members.
     * @param op The optional comparison operator.
     * @param patterns The list of patterns that this branch matches against.
     * @param guard The optional guard for the case branch.
     * @param body The body of the case branch.
     */
    CaseExpressionBranchAst(
        decltype(Op) &&op,
        decltype(Patterns) &&patterns,
        decltype(Guard) &&guard,
        decltype(Body) &&body);

    ~CaseExpressionBranchAst() override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

private:
    /**
     * If there are multiple patterns, then the llvm output value is a logical OR of all the pattern matches. This is
     * because the semantic analysis will have guaranteed that all the expressions are boolean. If there is only 1
     * pattern, such as with case-of patterns, then this pattern's codegen is returned directly.
     * @param sm The scope manager.
     * @param meta Associated metadata.
     * @param ctx The llvm code generation context.
     * @return The llvm value representing the combined pattern matches.
     */
    auto _CodegenCombinePatterns(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) const -> llvm::Value*;
};
