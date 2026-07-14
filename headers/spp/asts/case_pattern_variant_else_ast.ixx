module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantElseAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::CasePatternVariantElseAst final : CasePatternVariantAst {
    /**
     * The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    Unique<TokenAst> TokElse;

    /**
     * Construct the CasePatternVariantElseAst with the arguments matching the members.
     * @param tok_else The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    explicit CasePatternVariantElseAst(
        decltype(TokElse) &&tok_else);

    ~CasePatternVariantElseAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto MarkForIterLoopExit() -> void;

    SPP_ATTR_NODISCARD auto MarkedForIterLoopExit() const -> bool;

private:
    bool _ForIterLoopExit = false;
};
