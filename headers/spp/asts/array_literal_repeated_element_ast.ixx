module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_repeated_element_ast;
import spp.asts.array_literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The ArrayLiteralRepeatedElementAst represents an array literal with repeated elements and a size. This is used to
 * create fixed-length arrays where the length is known at compile time, and the element's type superimposes `Copy`.
 *
 * @n
 * All accesses into this literal will return the @c None variant of the @code Opt[T]@endcode type, @c T being the
 * element type, until values are set into the slots of the array. As there are no elements when created, and full type
 * information is required on declaration, the element type and size must be specified.
 */
SPP_EXP_CLS struct spp::asts::ArrayLiteralRepeatedElementAst final : ArrayLiteralAst {
    SPP_GCC_VTABLE_FIX
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left square bracket @code [@endcode in the array literal. This introduces the array
     * literal.
     */
    Unique<TokenAst> TokL;

    /**
     * The repeated element for the array. Every element in the array will be this value, by copying and placing in
     * contiguous memory.
     */
    Unique<ExpressionAst> Elem;

    /**
     * Separator token that represents the comma @code ;@endcode in the array literal. Separates the element value from
     * the number of elements this array will hold.
     */
    Unique<TokenAst> TokSemicolon;

    /**
     * The number of elements in the array. The mapped type is @code std::Arr[T, n]@endcode, a fixed length array, so
     * the length must be known at compile time.
     */
    Unique<ExpressionAst> Size;

    /**
     * The token that represents the right square bracket @code ]@endcode in the array literal. This closes the array
     * literal.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the ArrayLiteral0Elements with the arguments matching the members.
     * @param[in] tok_l The token that represents the left square bracket @code [@endcode in the array literal.
     * @param[in] elem The repeated element for the array.
     * @param[in] tok_semicolon Separator token that represents the comma @code ;@endcode in the array literal.
     * @param[in] size The number of elements in the array.
     * @param[in] tok_r The token that represents the right square bracket @code ]@endcode in the array literal.
     */
    ArrayLiteralRepeatedElementAst(
        decltype(TokL) &&tok_l,
        decltype(Elem) &&elem,
        decltype(TokSemicolon) &&tok_semicolon,
        decltype(Size) &&size,
        decltype(TokR) &&tok_r);

    ~ArrayLiteralRepeatedElementAst() override;

    /**
     * Check the repeated element for equality with the repeated element in the other array literal, and check the size
     * for equality with the size in the other array literal. The array literals are only equal if both the repeated
     * elements and the sizes are equal. Given this is only used in compile-time contexts, the repeated element and size
     * will be evaluatable to the AST.
     * @param other The other array literal to compare with.
     * @return @code Ordering::equal@endcode if the array literals are equal, and
     * @code Ordering::less@endcode otherwise.
     */
    SPP_ATTR_NODISCARD auto EqualsArrayLiteralRepeatedElement(ArrayLiteralRepeatedElementAst const &other) const -> Ordering override;

    /**
     * Reverse hook to equate against the other arguments. This will call the @c equals_array_literal_repeated_elements
     * method on the other expression, if it is an array literal with repeated elements, to check for equality.
     * @param other
     * @return @code Ordering::equal@endcode if the array literals are equal, and
     * @code Ordering::less@endcode otherwise.
     */
    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    /**
     * Ensure that the element given is both copyable and not a borrow type, allowing it to be stored as multiple
     * elements int he array. This is because the element is copied using @c memcpy @c n times into the array. Standard
     * memory rules prevent borrows from being stored in arrays. The @n argument is verified to be a const variable if
     * it is symbolic.
     * @param[in] sm The scope manager to use for type checking.
     * @param[in,out] meta Associated metadata.
     */
    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Check the memory of the elements inside the array literal. This is done by checking calling the same method on
     * each element.
     * @param sm The scope manager to use for memory checking.
     * @param meta Associated metadata.
     */
    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Resolve the array literal at compile time. This is only possible if both the element and the size are compile
     * time resolvable themselves.
     * @param sm The scope manager to use for resolution.
     * @param meta Associated metadata.
     * @return The compile time resolved array literal.
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
     * the element in the array literal, and @c n is the size given.
     * @param [in] sm The scope manager to use for type inference.
     * @param [in,out] meta Associated metadata.
     * @return The @code std::array::Arr[T, n]@endcode type of the array literal.
     */
    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ArrayLiteralRepeatedElementAst)
