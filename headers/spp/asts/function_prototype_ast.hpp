#ifndef FUNCTION_PROTOTYPE_HPP
#define FUNCTION_PROTOTYPE_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The FunctionPrototypeAst represents the prototype of a function. It defines the structure of a function, including
 * its name, parameters, and return type. The body of the function is defined in the FunctionImplementationAst.
 *
 * @n
 * This ASt is further inherited into the SubroutinePrototypeAst and CoroutinePrototypeAst, which add additional
 * analysis checks.
 */
struct spp::asts::FunctionPrototypeAst : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;
    friend struct AnnotationAst;

private:
    /**
     * Save the original function name prior to AST transformations.
     */
    IdentifierAst *m_orig_name;

    /**
     * Optional @c @abstractmethod annotation. This is used to indicate that the function is abstract and must be
     * implemented in subclasses.
     */
    AnnotationAst *m_abstract_annotation;

    /**
     * Optional @c @virtualmethod annotation. This is used to indicate that the function is virtual and can be
     * overridden in subclasses.
     */
    AnnotationAst *m_virtual_annotation;

    /**
     * Optional @c @hot or @c @cold annotation. This is used to indicate that the function is a hot/cold function,
     * which means it is called frequently/infrequently and should be optimized for performance.
     */
    AnnotationAst *m_temperature_annotation;

    /**
     * Optional @c @no_impl annotation. This is used to indicate that the function is not implemented, and the usual
     * type checking rules can be suspended (but this function cannot be called).
     */
    AnnotationAst *m_no_impl_annotation;

    /**
     * Optional @c @always_inline, @c @inline, or @c @no_inline annotation. This is used to indicate that the function
     * should be inlined, or not inlined, or always inlined.
     */
    AnnotationAst *m_inline_annotation;

public:
    /**
     * The list of annotations that are applied to this function prototype. There are quite a lot of annotations that
     * can be applied here, including the typical access modifiers, but also @c @virtualmethod, @c @abstractmethod, and
     * @c @hot/@cold.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The @c fun or @cor keyword that represents the start of the function prototype. This is used to indicate that a
     * function is being defined.
     */
    std::unique_ptr<TokenAst> tok_fun;

    /**
     * The name of the function prototype. This is the identifier that is used to refer to the function. Uniqueness is
     * not strictly required, as overloading is supported.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * The optional generic parameter group for the function prototype. This is used to define generic types that the
     * function can use. This is not required, and can be omitted if the function does not use generics.
     */
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;

    /**
     * The parameter group for the function prototype. This is used to define the parameters that the function takes.
     * This is required, and must be present in the function prototype.
     */
    std::unique_ptr<FunctionParameterGroupAst> param_group;

    /**
     * The token that represents the arrow \c -> in the function prototype. This separates the parameters from the
     * return type.
     */
    std::unique_ptr<TokenAst> tok_arrow;

    /**
     * The return type of the function prototype. This is the type that the function will return. This is required, and
     * is never "inferrable" from the expressions inside the function.
     */
    std::unique_ptr<TypeAst> return_type;

    /**
     * The implementation of the function prototype. This is the body of the function, and contains the actual code that
     * will be executed when the function is called.
     */
    std::unique_ptr<FunctionImplementationAst> impl;

    /**
     * Construct the FunctionPrototypeAst with the arguments matching the members.
     * @param annotations The list of annotations that are applied to this function prototype.
     * @param tok_fun The @c fun or @cor keyword that represents the start of the function prototype.
     * @param name The name of the function prototype.
     * @param generic_param_group An optional generic parameter group for the function prototype.
     * @param param_group The parameter group for the function prototype.
     * @param tok_arrow The token that represents the arrow \c -> in the function prototype.
     * @param return_type The return type of the function prototype.
     * @param impl The implementation of the function prototype.
     */
    FunctionPrototypeAst(
        decltype(annotations) &&annotations,
        decltype(tok_fun) &&tok_fun,
        decltype(name) &&name,
        decltype(generic_param_group) &&generic_param_group,
        decltype(param_group) &&param_group,
        decltype(tok_arrow) &&tok_arrow,
        decltype(return_type) &&return_type,
        decltype(impl) &&impl);

    ~FunctionPrototypeAst() override;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //FUNCTION_PROTOTYPE_HPP
