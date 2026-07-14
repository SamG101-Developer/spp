module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureObjectAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableDestructureObjectAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureObjectAst final : LocalVariableAst {
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
    Vec<Unique<LocalVariableAst>> Elems;

    /**
     * The @code )@endcode token that indicates the end of an object destructuring pattern.
     */
    Unique<TokenAst> TokR;

    struct {
        Unique<TypeAst> OriginalType;
    } Source;

    /**
     * Construct the LocalVariableDestructureObjectAst with the arguments matching the members.
     * @param[in] type The type of the object being destructured.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a object destructuring pattern.
     * @param[in] elems The elements of the object destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a object destructuring pattern.
     */
    LocalVariableDestructureObjectAst(
        decltype(Type) &&type,
        decltype(TokL) &&tok_l,
        decltype(Elems) &&elems,
        decltype(TokR) &&tok_r);

    ~LocalVariableDestructureObjectAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto ExtractName() const -> IdentifierAst* override;

private:
    Vec<Unique<LetStatementInitializedAst>> _NewAsts;
    analyse::scopes::VariableSymbol *_CondSym;
    analyse::scopes::VariableSymbol *_FlowSym;
    Unique<LetStatementInitializedAst> _CondLet;
};
