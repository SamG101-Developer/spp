#pragma once
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/mixins/visbility_enabled_ast.hpp>
#include <spp/asts/module_member_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>


namespace spp::analyse::scopes {
    struct AliasSymbol;
}

/**
 * The TypeStatementAst is used to alias a type to a new name in this scope. It can also use generic parameters for more
 * complex types, such as aliasing vectors, or partially specialized hash maps etc. For example,
 * @code type SecureByteMap[T] = std::collections::HashMap[K=Byte, V=T, A=SecureAlloc[(K, V)]]@endcode
 */
struct spp::asts::TypeStatementAst final : StatementAst, mixins::VisibilityEnabledAst, SupMemberAst, ModuleMemberAst {
    SPP_AST_KEY_FUNCTIONS;
    friend struct UseStatementAst;

private:
    bool m_generated = false;

    analyse::scopes::AliasSymbol *m_alias_sym;

    std::unique_ptr<ClassPrototypeAst> m_generated_cls_ast;

    std::unique_ptr<SupPrototypeExtensionAst> m_generated_ext_ast;

public:
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
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;

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

private:
    auto m_skip_all_scopes(ScopeManager *sm) -> void;

public:
    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
