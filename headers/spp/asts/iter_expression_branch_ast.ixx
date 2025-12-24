module;
#include <spp/macros.hpp>

export module spp.asts.iter_expression_branch_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IterExpressionBranchAst;
    SPP_EXP_CLS struct IterPatternVariantAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct PatternGuardAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::IterExpressionBranchAst final : virtual Ast, mixins::TypeInferrableAst {
    /**
     * The pattern that this branch matches against. This can only be a single pattern due to different @c iter patterns
     * introducing different variables and symbols (same as case-of destructuring).
     */
    std::unique_ptr<IterPatternVariantAst> pattern;

    /**
     * An optional guard for the iteration branch. This is a boolean expression that must evaluate to true for the
     * branch to be executed.
     */
    std::unique_ptr<PatternGuardAst> guard;

    /**
     * The body of the iteration branch. This is an inner scope that contains the statements that will be executed if
     * the branch matches.
     */
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> body;

    /**
     * Construct the IterExpressionBranchAst with the arguments matching the members.
     * @param pattern The pattern that this branch matches against.
     * @param guard The optional guard for the iteration branch.
     * @param body The body of the iteration branch.
     */
    IterExpressionBranchAst(
        decltype(pattern) &&pattern,
        decltype(guard) &&guard,
        decltype(body) &&body);

    ~IterExpressionBranchAst() override;

    SPP_AST_KEY_FUNCTIONS;

private:
    auto m_codegen_combine_patterns(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) const -> llvm::Value*;

public:
    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
