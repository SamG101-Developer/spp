#pragma once
#include <spp/asts/ast.hpp>


#define SPP_NO_ANNOTATIONS std::vector<std::unique_ptr<asts::AnnotationAst>>()


/**
 * An AnnotationAst is used to represent a non-code generated transformation of behaviour inside an AST. For example,
 * marking a method as @c @virtualmethod won't generate any code, but will tag the method as virtual, unlocking
 * additional behaviour in the compiler.
 */
struct spp::asts::AnnotationAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the @c @ sign in the annotation. This introduces the annotation.
     */
    std::unique_ptr<TokenAst> tok_at_sign;

    /**
     * The name of the annotation. This is the identifier that follows the @c @ sign.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * Construct the AnnotationAst with the arguments matching the members.
     * @param[in] tok_at_sign The token that represents the @c @ sign in the annotation.
     * @param[in] name The name of the annotation.
     */
    AnnotationAst(
        decltype(tok_at_sign) &&tok_at_sign,
        decltype(name) &&name);

    ~AnnotationAst() override;

    /**
     * Custom comparison involves comparing the identifier of the annotation. This makes checking for duplicate
     * annotations easier.
     * @return Whether the identifiers of the annotations are equal.
     */
    auto operator==(AnnotationAst const &that) const -> bool;

    /**
     * Preprocessing an @c AnnotationAst involves setting tags on certain contextual ASTs, if they are the correct type.
     * Errors are not thrown for incorrect contextual ASTs, as a scope manager is not present at this stage, so cannot
     * provide the correct error formatter to the semantic error being thrown.
     * @param[in] ctx The context that this annotation is being applied to.
     */
    auto stage_1_pre_process(Ast *ctx) -> void override;

    /**
     * There are three key checks that are performed in this stage:
     *      1. The context AST is checked to be of a compatible type for the annotation. For example, a
     *      @c @virtualmethod annotation can only be applied to a method, and not a free function or class.
     *      2. The annotation is checked for conflicting annotations on the same context AST. For example, a
     *      @c @public annotation cannot be applied to a method that is already marked as @c @private.
     *      3. The annotation is checked to be a builtin annotation, that is known to the compiler. Custom annotations
     *      are not supported.
     * @param[in] sm The scope manager to check the context AST against.
     * @param[in,out] meta Associated metadata.
     */
    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
