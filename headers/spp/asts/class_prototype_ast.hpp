#ifndef CLASS_PROTOTYPE_AST_HPP
#define CLASS_PROTOTYPE_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


namespace spp::analyse::scopes {
    struct TypeSymbol;
}

/**
 * The ClassPrototypeAst represents the prototype of a class in the abstract syntax tree. It defines the structure of a
 * class, including its name and any generic parameters it may have. The attributes are defined in the implementation
 * ast for this class, allowing for scoping rules to be made easier.
 */
struct spp::asts::ClassPrototypeAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

protected:
    bool m_for_alias = false;

    analyse::scopes::TypeSymbol* m_cls_sym = nullptr;

public:
    /**
     * The list of annotations that are applied to this class prototype. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The @c cls keyword that represents the start of the class prototype. This is used to indicate that a class is
     * being defined.
     */
    std::unique_ptr<TokenAst> tok_cls;

    /**
     * The name of the class prototype. This is the identifier that is used to refer to the class, and must be unique
     * within the scope.
     */
    std::shared_ptr<TypeAst> name;

    /**
     * An optional generic parameter group for the class prototype. This is used to define generic types that the class
     * can use.
     */
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;

    /**
     * The list of class attributes that are defined on the class prototype. These are the properties that the class
     * will have, and can be accessed through instances of the class.
     */
    std::unique_ptr<ClassImplementationAst> impl;

    /**
     * Construct the ClassPrototypeAst with the arguments matching the members.
     * @param[in] annotations The list of annotations that are applied to this class prototype.
     * @param[in] tok_cls The @c cls keyword that represents the start of the class prototype.
     * @param[in] name The name of the class prototype.
     * @param[in] generic_param_group An optional generic parameter group for the class prototype.
     * @param[in] impl The list of class attributes that are defined on the class prototype.
     */
    ClassPrototypeAst(
        decltype(annotations) &&annotations,
        decltype(tok_cls) &&tok_cls,
        decltype(name) &&name,
        decltype(generic_param_group) &&generic_param_group,
        decltype(impl) &&impl);

protected:
    auto m_generate_symbols(ScopeManager* sm) -> analyse::scopes::TypeSymbol*;

public:
    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //CLASS_PROTOTYPE_AST_HPP
