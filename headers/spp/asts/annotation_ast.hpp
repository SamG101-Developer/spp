#ifndef ANNOTATION_AST_HPP
#define ANNOTATION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


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
    std::unique_ptr<IdentifierAst> name;

    /**
     * Construct the AnnotationAst with the arguments matching the members.
     * @param[in] tok_at_sign The token that represents the @c @ sign in the annotation.
     * @param[in] name The name of the annotation.
     */
    AnnotationAst(
        decltype(tok_at_sign) &&tok_at_sign,
        decltype(name) &&name);

    /**
     * Custom comparison involves comparing the identifier of the annotation. This makes checking for duplicate
     * annotations easier.
     * @return Whether the identifiers of the annotations are equal.
     */
    auto operator==(AnnotationAst const &that) const -> bool;
};


#endif //ANNOTATION_AST_HPP
