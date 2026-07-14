module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureObjectAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureObjectAst final : CasePatternVariantAst {
    /**
     * The type of the object being destructured. This is used to determine the type of the destructured elements (by
     * attribute type inference)
     */
    Unique<TypeAst> Type;

    /**
     * The @code (@endcode token that indicates the start of a object destructuring pattern.
     */
    Unique<TokenAst> TokL;

    /**
     * The elements of the object destructuring pattern. This is a list of patterns that will be destructured from the
     * object. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    UniqueVec<CasePatternVariantAst> Elems;

    /**
     * The @code )@endcode token that indicates the end of an object destructuring pattern.
     */
    Unique<TokenAst> TokR;

    struct {
        Unique<TypeAst> OriginalType;
    } Source;

    /**
     * Construct the CasePatternVariantDestructureObjectAst with the arguments matching the members.
     * @param[in] type The type of the object being destructured.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a object destructuring pattern.
     * @param[in] elems The elements of the object destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a object destructuring pattern.
     */
    CasePatternVariantDestructureObjectAst(
        decltype(Type) type,
        decltype(TokL) &&tok_l,
        decltype(Elems) &&elems,
        decltype(TokR) &&tok_r);

    ~CasePatternVariantDestructureObjectAst() override;

    static auto FromType(
        Unique<TypeAst> &&type)
        -> Unique<CasePatternVariantDestructureObjectAst>;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto ConvToVar(meta::CompilerMetaData *meta) -> Unique<LocalVariableAst> override;

private:
    analyse::scopes::VariableSymbol *_CondSym;
    analyse::scopes::VariableSymbol *_FlowSym;
};
