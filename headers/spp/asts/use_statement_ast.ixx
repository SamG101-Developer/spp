module;
#include <spp/macros.hpp>

export module spp.asts.use_statement_ast;
import spp.asts.statement_ast;
import spp.asts.module_member_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeStatementAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct UseStatementAst;
}


/**
 * The UseStatementAst reduces a fully qualified type into the current scope, making the symbol accessible without the
 * associated namespace. It is internal mapped to a TypeStatementAst: @code use std::Str@endcode is equivalent to
 * @code type Str = std::Str@endcode.
 */
SPP_EXP_CLS struct spp::asts::UseStatementAst final : StatementAst, ModuleMemberAst {
    /**
     * The list of annotations that are applied to this use statement. Typically, access modifiers in this context.
     */
    Vec<Unique<AnnotationAst>> Annotations;

    /**
     * The @c use token that starts this statement.
     */
    Unique<TokenAst> TokUse;

    /**
     * The old (fully qualified) type that this use statement is reducing. For example, for @code use
     * Str::Str@endcode, the fully qualified type is @c Str::Str.
     */
    Shared<TypeAst> OldType;

    /**
     * Construct the UseStatementAst with the arguments matching the members.
     * @param annotations The list of annotations that are applied to this use statement.
     * @param tok_use The @c use token that starts this statement.
     * @param old_type The old (fully qualified) type that this use statement is reducing.
     */
    UseStatementAst(
        decltype(Annotations) &&annotations,
        decltype(TokUse) &&tok_use,
        decltype(OldType) old_type);

    ~UseStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

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
    Unique<TypeStatementAst> _Conversion;
};
