#pragma once
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/mixins/visbility_enabled_ast.hpp>


/**
 * The ClassAttributeAst represents an attribute of a class. It is defined on the class prototype ast, and is used to
 * add "state" to a type.
 */
struct spp::asts::ClassAttributeAst final : virtual Ast, mixins::VisibilityEnabledAst, ClassMemberAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The list of annotations that are applied to this class attribute. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The name of the class attribute. This is the identifier that is used to refer to the attribute on the class, and
     * must be unique to the class.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * The token that represents the colon @c : in the class attribute definition. This separates the name from the type.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The type of the class attribute. This is the type that the attribute will hold, and must be specified.
     */
    std::shared_ptr<TypeAst> type;

    /**
     * An optional default value for the class attribute. This is the value that will be assigned to the attribute if no
     * value is provided when creating an instance of the class. Otherwise, standard "default initialization" will be
     * used, which is the default value of the type.
     */
    std::unique_ptr<ExpressionAst> default_val;

    /**
     * Construct the ClassAttributeAst with the arguments matching the members.
     * @param[in] annotations The list of annotations that are applied to this class attribute.
     * @param[in] name The name of the class attribute.
     * @param[in] tok_colon The token that represents the colon @c : in the class attribute definition.
     * @param[in] type The type of the class attribute.
     * @param[in] default_val An optional default value for the class attribute.
     */
    ClassAttributeAst(
        decltype(annotations) &&annotations,
        decltype(name) &&name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type,
        decltype(default_val) &&default_val);

    ~ClassAttributeAst() override;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
