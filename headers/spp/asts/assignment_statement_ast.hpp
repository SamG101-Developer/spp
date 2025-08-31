#ifndef ASSIGNMENT_STATEMENT_AST_HPP
#define ASSIGNMENT_STATEMENT_AST_HPP

#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * An assignment statement allows for one value to be moved or copied into another variable or property (or multiple
 * values into multiple variables or properties at once). Only symbolic variables and properties can be assigned to.
 *
 * @note Compound assignment expressions, such as @c += or @c -=, are classified under @c BinaryExpressionAst nodes,
 * because they share function mapping logic with binary expressions, and can only have 1 left-hand-side expression
 * anyway. Some checks are copied from this class (symbolic target, etc.).
 */
struct spp::asts::AssignmentStatementAst final : StatementAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The list of left-hand side expressions in the assignment statement. These are the variables or properties that
     * are being assigned a value.
     */
    std::vector<std::unique_ptr<ExpressionAst>> lhs;

    /**
     * The @c = token that represents the assignment operator. This indicates to the parser that an assignment statement
     * has been defined.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The list of right-hand side expressions in the assignment statement. These are the values or expressions that
     * are being assigned to the left-hand side variables or properties.
     */
    std::vector<std::unique_ptr<ExpressionAst>> rhs;

    /**
     * Construct the AssignmentStatementAst with the arguments matching the members.
     * @param[in] lhs The list of left-hand side expressions in the assignment statement.
     * @param[in] tok_assign The @c = token that represents the assignment operator.
     * @param[in] rhs The list of right-hand side expressions in the assignment statement.
     */
    AssignmentStatementAst(
        decltype(lhs) &&lhs,
        decltype(tok_assign) &&tok_assign,
        decltype(rhs) &&rhs);

    ~AssignmentStatementAst() override;

    /**
     * An assignment statement ast node must have symbolic left-hand-side nodes. This ensures that the left-hand-side
     * expressions are non-temporary, and therefore have valid locations in emory for assignment. Assigning to
     * non-symbolic nodes is semantically nonsensical and therefore prohibited, The types must match for each
     * left-hand-side and right-hand-side expression. The right-hand-side symbols' memory statuses must also be valid
     * for assignment (fully initialized).
     *
     * Mutability checks ensure that the left-hand-side expressions are allowed to be mutated and there are three
     * different checks that can take place depending on whether the left-hand-side part being checked is a variable
     * or attribute (both are "symbolic"):
     *      1. For a variable, the symbol must be marked as "mut" , or never been initialized before.
     *      2. For a [non-borrowed variable]'s attribute, the outermost symbol must be marked as "mut".
     *      3. For a [borrowed variable]'s attribute, the outermost symbol must be a "&mut" borrow, or the attribute.
     *
     * Because that the mutability of an attribute is dependent on the mutability of the outermost variable containing
     * the attribute, no additional or nested mutability checks are performed.
     * @param[in] sm The scope manager to find the symbols of the left-hand-side and right-hand-side expressions in.
     * @param[in,out] meta Associated metadata.
     * @throw spp::analyse::errors::SppTypeMismatchError if the right-hand-side expressions do not match the corresponding
     * left-hand-side expressions.
     * @throw spp::analyse::errors::SppAssignmentToNonSymbolicError if any of the left-hand-side expressions are not
     * symbolic.
     * @throw spp::analyse::errors::SppInvalidMutationError if any of the left-hand-side expressions are not mutable.
     */
    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    /**
     * The memory checks for assignment statements fall under the following categories:
     *      1. Ensure the left-hand-side is initialized for an attribute initialization.
     *      2. Ensure the right-hand-side is initialized.
     *      3. Resolve moves if an identifier is set to a new value.
     *      4. Resolve partial moves if an attribute is set to a new value.
     *
     * The left-hand-side symbol may be non-initialized as the entire symbol is being set to a new value. This is how
     * non-initialized/moved symbols become re-initialized with a new value. If the left-hand-side's direct outer
     * symbol (of a target attribute) is non-initialized, then a single attribute cannot be set onto it, as it has no
     * location in memory. For an attribute to be set a new value, the outermost variable must be initialized. For
     * example, if "a.b.c = ..." is the assignment, then "a.b" must be initialized, ie "a" is initialized, and "a.b" is
     * not in the partial moves list of the symbol for the variable "a".
     *
     * If a value has been set a new value, then mark this symbol as initialized, with this statement being the AST
     * that most recently initialized it. Otherwise, an attribute has been set, so resolve this partial move (if it is
     * a partial move) on the outermost symbol. If there is no outermost symbol, then the target is  "temporary" value,
     * and as such requires no further action.
     * @param[in] sm The scope manager to find the symbols of the left-hand-side and right-hand-side expressions in.
     * @param[in,out] meta Associated metadata.
     */
    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //ASSIGNMENT_STATEMENT_AST_HPP
