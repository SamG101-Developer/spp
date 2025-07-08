#ifndef TYPE_STATEMENT_AST_HPP
#define TYPE_STATEMENT_AST_HPP

#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The TypeStatementAst is used to alias a type to a new name in this scope. It can also use generic parameters for more
 * complex types, such as aliasing vectors, or partially specialized hash maps etc. For example,
 * @code type SecureByteMap[T] = std::collections::HashMap[K=Byte, V=T, A=SecureAlloc[(K, V)]]@endcode
 */
struct spp::asts::TypeStatementAst final : StatementAst {
    SPP_AST_KEY_FUNCTIONS;

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
    std::unique_ptr<TypeAst> new_type;

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
    std::unique_ptr<TypeAst> old_type;

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
        decltype(new_type) &&new_type,
        decltype(generic_param_group) &&generic_param_group,
        decltype(tok_assign) &&tok_assign,
        decltype(old_type) &&old_type);

private:
    bool m_generated = false;

    // todo: other private members.
};


#endif //TYPE_STATEMENT_AST_HPP
