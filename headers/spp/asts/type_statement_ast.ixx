module;
#include <spp/macros.hpp>

export module spp.asts.type_statement_ast;
import spp.analyse.scopes.scope;
import spp.asts.annotation_ast;
import spp.asts.statement_ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.asts.token_ast;
import spp.asts.mixins.visibility_enabled_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeStatementAst;
    SPP_EXP_CLS struct UseStatementAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    extern "C++" struct TypeSymbol;
}


/**
 * The TypeStatementAst is used to alias a type to a new name in this scope. It can also use generic parameters for more
 * complex types, such as aliasing vectors, or partially specialized hash maps etc. For example,
 * @code type SecureByteMap[T] = std::collections::HashMap[K=Byte, V=T, A=SecureAlloc[(K, V)]]@endcode
 */
SPP_EXP_CLS struct spp::asts::TypeStatementAst final : StatementAst, ModuleMemberAst, SupMemberAst, mixins::VisibilityEnabledAst {
private:
    bool m_generated;

    bool m_from_use_statement;

    std::shared_ptr<analyse::scopes::TypeSymbol> m_alias_sym;

public:
    analyse::scopes::Scope *m_temp_scope_1;
    analyse::scopes::Scope *m_temp_scope_2;
    std::unique_ptr<analyse::scopes::Scope> m_temp_scope_3;

    /**
     * The list of annotations that are applied to this type statement. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The @c type token that starts this statement.
     */
    std::unique_ptr<TokenAst> tok_type;

    /**
     * The type that this type statement is defining. For example, for @code type Str = std::Str@endcode, the
     * @c new_type is @c Str.
     */
    std::shared_ptr<TypeIdentifierAst> new_type;

    /**
     * The generic parameter group for the new type. For example,
     * @code type MyVector[T] = std::Vector[T, A=SomeAlloc]@endcode defines @c T as a generic internal to this type
     * statement only.
     */
    std::shared_ptr<GenericParameterGroupAst> generic_param_group;

    /**
     * The @c = token that separates the new type from the old type.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The old (fully qualified) type that this type statement is defining. For example, for
     * @code type Str = std::Str@endcode, the fully qualified type is @c std::Str.
     */
    std::shared_ptr<TypeAst> old_type;

    /**
     * Construct the TypeStatementAst with the arguments matching the members.
     * @param annotations The list of annotations that are applied to this type statement.
     * @param tok_type The @c type token that starts this statement.
     * @param new_type The type that this type statement is defining.
     * @param generic_param_group The generic parameter group for the new type.
     * @param tok_assign The @c = token that separates the new type from the old type.
     * @param old_type The old (fully qualified) type that this type statement is defining.
     */
    TypeStatementAst(
        decltype(annotations) &&annotations,
        decltype(tok_type) &&tok_type,
        decltype(new_type) new_type,
        decltype(generic_param_group) &&generic_param_group,
        decltype(tok_assign) &&tok_assign,
        decltype(old_type) old_type);

    ~TypeStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto mark_from_use_statement() -> void;

    auto cleanup() const -> void;
};


spp::asts::TypeStatementAst::~TypeStatementAst() {
    cleanup();
}
