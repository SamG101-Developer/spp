module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantLiteralAst;
    SPP_EXP_CLS struct LiteralAst;
    SPP_EXP_CLS struct LocalVariableAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::CasePatternVariantLiteralAst final : CasePatternVariantAst {
    /**
     * The literal value of the case pattern variant. This can be a string, integer, float, boolean, but not a tuple or
     * array; special destructure syntax exists for those literals.
     */
    Unique<LiteralAst> Literal;

    /**
     * Construct the CasePatternVariantLiteralAst with the arguments matching the members.
     * @param literal The literal value of the case pattern variant.
     */
    explicit CasePatternVariantLiteralAst(
        decltype(Literal) &&literal);

    ~CasePatternVariantLiteralAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto ConvToVar(meta::CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
