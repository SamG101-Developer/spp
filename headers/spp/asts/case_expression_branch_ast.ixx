module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_branch_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct PatternGuardAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::CaseExpressionBranchAst final : virtual Ast, mixins::TypeInferrableAst {
    /**
     * The optional comparison operator. This is for cases pattern matching cases that look something like
     * @code== 123 { ... }@endcode.
     */
    std::unique_ptr<TokenAst> op;

    /**
     * The list of patterns that this branch matches against. There can be more than 1 pattern for non-destructuring
     * operations.
     */
    std::vector<std::unique_ptr<CasePatternVariantAst>> patterns;

    /**
     * The optional guard for the case branch. This is a boolean expression that must evaluate to true for destructuring
     * patterns only.
     */
    std::unique_ptr<PatternGuardAst> guard;

    /**
     * The body of the case branch. This is an inner scope that contains the statements that will be executed if the
     * branch matches.
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * Construct the CaseExpressionBranchAst with the arguments matching the members.
     * @param op The optional comparison operator.
     * @param patterns The list of patterns that this branch matches against.
     * @param guard The optional guard for the case branch.
     * @param body The body of the case branch.
     */
    CaseExpressionBranchAst(
        decltype(op) &&op,
        decltype(patterns) &&patterns,
        decltype(guard) &&guard,
        decltype(body) &&body);

    ~CaseExpressionBranchAst() override;

    SPP_AST_KEY_FUNCTIONS;

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
    auto m_codegen_combine_patterns(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value*;

public:
    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
