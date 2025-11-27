module;
#include <spp/macros.hpp>

export module spp.asts.use_statement_ast;
import spp.asts.statement_ast;
import spp.asts.module_member_ast;

import std;


/**
 * The UseStatementAst reduces a fully qualified tpy into the current scope, making the symbol accessible without the
 * associated namespace. It is internal mapped to a TypeStatementAst: @code use std::Str@endcode is equivalent to
 * @code type Str = std::Str@endcode.
 */
SPP_EXP_CLS struct spp::asts::UseStatementAst final : StatementAst, ModuleMemberAst {
private:
    /**
     * The @c m_generated flag indicates whether this use statement has been generated yet. This is required, because
     * @c use statements can be defined at the top level (module/sup) or inside function bodies. If defined inside a
     * function body, all steps of the analysis must be run together, otherwise they are ran in their correct layer.
     */
    bool m_generated = false;

    /**
     * The @c m_conversion is the type statement that is generated from this use statement. It is used to analyse new
     * types in a uniform way with @code type Str = std::Str@endcode.
     */
    std::unique_ptr<TypeStatementAst> m_conversion;

public:
    /**
     * The list of annotations that are applied to this use statement. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The @c use token that starts this statement.
     */
    std::unique_ptr<TokenAst> tok_use;

    /**
     * The old (fully qualified) type that this use statement is reducing. For example, for @code use std::Str@endcode,
     * the fully qualified type is @c std::Str.
     */
    std::shared_ptr<TypeAst> old_type;

    /**
     * Construct the UseStatementAst with the arguments matching the members.
     * @param annotations The list of annotations that are applied to this use statement.
     * @param tok_use The @c use token that starts this statement.
     * @param old_type The old (fully qualified) type that this use statement is reducing.
     */
    UseStatementAst(
        decltype(annotations) &&annotations,
        decltype(tok_use) &&tok_use,
        decltype(old_type) old_type);

    ~UseStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
