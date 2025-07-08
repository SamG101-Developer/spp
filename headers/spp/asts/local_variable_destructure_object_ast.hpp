#ifndef LOCAL_VARIABLE_DESTRUCTURE_OBJECT_AST_HPP
#define LOCAL_VARIABLE_DESTRUCTURE_OBJECT_AST_HPP

#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LocalVariableDestructureObjectAst final : LocalVariableAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The type of the object being destructured. This is used to determine the type of the destructured elements (by
     * attribute type inference)
     */
    std::unique_ptr<TypeAst> type;

    /**
     * The @code (@endcode token that indicates the start of a object destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the object destructuring pattern. This is a list of patterns that will be destructured from the
     * object. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    std::vector<std::unique_ptr<LocalVariableAst>> elems;

    /**
     * The @code )@endcode token that indicates the end of an object destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the LocalVariableDestructureObjectAst with the arguments matching the members.
     * @param[in] type The type of the object being destructured.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a object destructuring pattern.
     * @param[in] elems The elements of the object destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a object destructuring pattern.
     */
    LocalVariableDestructureObjectAst(
        decltype(type) &&type,
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elems,
        decltype(tok_r) &&tok_r);
};


#endif //LOCAL_VARIABLE_DESTRUCTURE_OBJECT_AST_HPP
