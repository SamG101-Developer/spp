#ifndef CMP_STATEMENT_AST_HPP
#define CMP_STATEMENT_AST_HPP


#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The CmpStatementAst represents a compile time definition statement at either the module or superimposition level. It
 * is analogous to Rust's "const" statement.
 */
struct spp::asts::CmpStatementAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The list of annotations that are applied to this cmp statement. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The token that represents the @c cmp keyword in the cmp statement. This is used to indicate that a compile time
     * definition is being made.
     */
    std::unique_ptr<TokenAst> tok_cmp;

    /**
     * The name of the cmp statement. This is the identifier that is used to refer to the compile time definition, and
     * must be unique within the scope.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * The token that represents the colon \c : in the cmp statement definition. This separates the name from the type.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The type of the cmp statement. This is the type that the compile time definition will hold, and must be
     * specified. Needs to be specified rather than inferred, because the type must be known at an early stage that
     * needs to be completed before type-inference can be considered.
     */
    std::unique_ptr<TypeAst> type;

    /**
     * The token that represents the assignment operator \c = in the cmp statement definition. This separates the type
     * from the value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The value of the cmp statement. This is the expression that will be evaluated at compile time, and must be
     * constant. It can be any expression that is valid in a compile time context, such as a literal or an object
     * initialization that only uses compile time values.
     */
    std::unique_ptr<ExpressionAst> value;

    /**
     * Construct the CmpStatementAst with the arguments matching the members.
     * @param[in] pos The position of the AST in the source code.
     * @param[in] annotations The list of annotations that are applied to this cmp statement.
     * @param[in] tok_cmp The token that represents the @c cmp keyword in the cmp statement.
     * @param[in] name The name of the cmp statement.
     * @param[in] tok_colon The token that represents the colon \c : in the cmp statement definition.
     * @param[in] type The type of the cmp statement.
     * @param[in] tok_assign The token that represents the assignment operator \c = in the cmp statement definition.
     * @param[in] value The value of the cmp statement.
     */
    CmpStatementAst(
        decltype(annotations) &&annotations,
        decltype(tok_cmp) &&tok_cmp,
        decltype(name) &&name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type,
        decltype(tok_assign) &&tok_assign,
        decltype(value) &&value);
};


#endif //CMP_STATEMENT_AST_HPP
