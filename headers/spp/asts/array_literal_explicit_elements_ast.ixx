module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The ArrayLiteralExplicitElementsAst represents an array literal with a variable number of elements. This is used to create
 * an @code std::Arr[T, n]@endcode type, with @c T being the inferred type of each element (must all be the same type,
 * and @c n being the number of elements provided.
 *
 * @n
 * The elements are stored as @c Ast* pointers, but are restricted to expression-like ASTs by the parser. Their
 * respective analysis functions will be called by inheritance/vtable logic.
 */
SPP_EXP_CLS struct spp::asts::ArrayLiteralExplicitElementsAst final : ArrayLiteralAst {
    SPP_GCC_VTABLE_FIX
    SPP_AST_KEY_FUNCTIONS

    /**
     * The token that represents the left square bracket @code [@endcode in the array literal. This introduces the array
     * literal.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of expressions that are the elements of the array. Each element is an AST that represents an expression.
     * They will all infer to the same type.
     */
    UniqueVec<ExpressionAst> Elems;

    /**
     * The token that represents the right square bracket @code ]@endcode in the array literal. This closes the array
     * literal.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the ArrayLiteralNElements with the arguments matching the members.
     * @param[in] tok_l The token that represents the left square bracket @c [ in the array literal.
     * @param[in] elements The list of expressions that are the elements of the array.
     * @param[in] tok_r The token that represents the right square bracket @c ] in the array literal.
     */
    ArrayLiteralExplicitElementsAst(
        decltype(TokL) &&tok_l,
        decltype(Elems) &&elements,
        decltype(TokR) &&tok_r);

    ~ArrayLiteralExplicitElementsAst() override;

    /**
     * Check each element for equality with the corresponding element in the other array literal. The array literals are
     * only equal if all the elements are equal. Given this is only used in compile-time contexts, each element will be
     * evaluatable to the AST.
     * @param other The other array literal to compare with.
     * @return @code Ordering::equal@endcode if the array literals are equal, and
     * @code Ordering::less@endcode otherwise.
     */
    SPP_ATTR_NODISCARD auto EqualsArrayLiteralExplicitElements(ArrayLiteralExplicitElementsAst const &other) const -> Ordering override;

    /**
     * Reverse hook to equate against the other arguments. This will call the @c equals_array_literal_explicit_elements
     * method on the other expression, if it is an array literal with explicit elements, to check for equality.
     * @param other The other expression to compare with.
     * @return @code Ordering::equal@endcode if the expressions are equal, and
     * @code Ordering::less@endcode otherwise.
     */
    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    /**
     * Semantic analysis for an array with explicit elements ensures that all elements are of the same type, and that
     * none of the elements are borrowed (ie the type of all the elements is not a borrow type). This is because it
     * would otherwise be a violation of the second class borrow memory safety model.
     * @param [in] sm The scope manager to find the symbols of the elements in.
     * @param [in,out] meta Associated metadata.
     * @throw spp::analyse::errors::SppTypeMismatchError if the elements are not of the same type.
     * @throw spp::analyse::errors::SppSecondClassBorrowViolationError if any of the elements are borrowed.
     */
    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Check the memory state of the element being repeated (mostly to ensure that it is initialised and not just a
     * valid type).
     * @param sm The scope manager to use for memory checking.
     * @param meta Associated metadata.
     */
    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Resolve the array literal at compile time. This is only possible if all the elements are compile time resolvable
     * themselves.
     * @param sm The scope manager to use for resolution.
     * @param meta Associated metadata.
     * @return The result of the compile time resolution.
     */
    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Create an array type based on the internal element type and the number of elements.
     * @param sm The scope manager to use for code generation.
     * @param meta Associated metadata.
     * @param ctx The LLVM context to use for code generation.
     * @return The LLVM value representing the array literal.
     */
    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    /**
     * The inferred type of an array literal is always @code std::array::Arr[T, n]@endcode, where @c T is the type of
     * the elements in the array literal, and @c n is the number of elements in the array literal.
     * @param [in] sm The scope manager to use for type inference.
     * @param [in,out] meta Associated metadata.
     * @return The @code std::array::Arr[T, n]@endcode type of the array literal.
     */
    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ArrayLiteralExplicitElementsAst)
