module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_array_ast;
import spp.asts.local_variable_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureArrayAst;
    SPP_EXP_CLS struct LocalVariableDestructureArrayAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct IdentifierAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureArrayAst final : LocalVariableAst {
    /**
     * The @code [@endcode token that indicates the start of an array destructuring pattern.
     */
    Unique<TokenAst> TokL;

    /**
     * The elements of the array destructuring pattern. This is a list of patterns that will be destructured from the
     * array. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    Vec<Unique<LocalVariableAst>> Elems;

    /**
     * The @code ]@endcode token that indicates the end of an array destructuring pattern.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the LocalVariableDestructureArrayAst with the arguments matching the members.
     * @param[in] tok_l The @code [@endcode token that indicates the start of an array destructuring pattern.
     * @param[in] elems The elements of the array destructuring pattern.
     * @param[in] tok_r The @code ]@endcode token that indicates the end of an array destructuring pattern.
     */
    LocalVariableDestructureArrayAst(
        decltype(TokL) &&tok_l,
        decltype(Elems) &&elems,
        decltype(TokR) &&tok_r);

    ~LocalVariableDestructureArrayAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto ExtractName() const -> IdentifierAst* override;

private:
    Vec<Unique<LetStatementInitializedAst>> _NewAsts;
};
