module;
#include <spp/macros.hpp>

export module spp.asts.use_statement_variable_ast;
import spp.asts.statement_ast;
import spp.asts.module_member_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct UseStatementVariableAst;
}

COMMON_AST_IMPORTS

/**
 * The UseStatementVariableAst reduces a fully qualified variable into the current scope, making the symbol accessible
 * without the associated namespace. Internal symbol mapping for variables or namespaces are used.
 */
SPP_EXP_CLS struct spp::asts::UseStatementVariableAst final : StatementAst, ModuleMemberAst {
    /**
     * The list of annotations that are applied to this use statement. Typically, access modifiers in this context.
     */
    Vec<Unique<AnnotationAst>> Annotations;

    /**
     * The @c use token that starts this statement.
     */
    Unique<TokenAst> TokUse;

    /**
     * The old (fully qualified) variable that this use statement is reducing. For example, for @code use
     * std::annotations::public@endcode, the fully qualified type is @c std::annotations::public.
     */
    Unique<ExpressionAst> OldVar;

    /**
     * Construct the UseStatementAst with the arguments matching the members.
     * @param annotations The list of annotations that are applied to this use statement.
     * @param tok_use The @c use token that starts this statement.
     * @param old_var The old (fully qualified) variable that this use statement is reducing.
     */
    UseStatementVariableAst(
        decltype(Annotations) &&annotations,
        decltype(TokUse) &&tok_use,
        decltype(OldVar) old_var);

    ~UseStatementVariableAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *) -> void override;

    auto Stage3_GenTopLvlAliases(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage6_PreAnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage10_PreCodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

private:
    /**
     * The @c m_generated flag indicates whether this use statement has been generated yet. This is required, because
     * @c use statements can be defined at the top level (module/sup) or inside function bodies. If defined inside a
     * function body, all steps of the analysis must be run together, otherwise they are ran in their correct layer.
     */
    bool _Generated = false;

    /**
     * The @c m_conversion is the type statement that is generated from this use statement. It is used to analyse new
     * types in a uniform way with @code type Str = std::Str@endcode.
     */
    Unique<CmpStatementAst> _Conversion;
};
