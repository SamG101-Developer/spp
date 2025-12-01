module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_repeated_element_ast;
import spp.asts.array_literal_ast;
import spp.asts.token_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
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
    /**
     * The token that represents the left square bracket @code [@endcode in the array literal. This introduces the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The repeated element for the array. Every element in the array will be this value, by copying and placing in
     * contiguous memory.
     */
    std::unique_ptr<ExpressionAst> elem;

    /**
     * Separator token that represents the comma @code ;@endcode in the array literal. Separates the element value from
     * the number of elements this array will hold.
     */
    std::unique_ptr<TokenAst> tok_semicolon;

    /**
     * The number of elements in the array. The mapped type is @code std::Arr[T, n]@endcode, a fixed length array, so
     * the length must be known at compile time.
     */
    std::unique_ptr<ExpressionAst> size;

    /**
     * The token that represents the right square bracket @code ]@endcode in the array literal. This closes the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ArrayLiteral0Elements with the arguments matching the members.
     * @param[in] tok_l The token that represents the left square bracket @code [@endcode in the array literal.
     * @param[in] elem The repeated element for the array.
     * @param[in] tok_semicolon Separator token that represents the comma @code ;@endcode in the array literal.
     * @param[in] size The number of elements in the array.
     * @param[in] tok_r The token that represents the right square bracket @code ]@endcode in the array literal.
     */
    ArrayLiteralRepeatedElementAst(
        decltype(tok_l) &&tok_l,
        decltype(elem) &&elem,
        decltype(tok_semicolon) &&tok_semicolon,
        decltype(size) &&size,
        decltype(tok_r) &&tok_r);

    ~ArrayLiteralRepeatedElementAst() override;

protected:
    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;
    auto equals_array_literal_repeated_elements(ArrayLiteralRepeatedElementAst const &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    /**
     * Ensure that the element given is both copyable and not a borrow type, allowing it to be stored as multiple
     * elements int he array. This is because the element is copied using @c memcpy @c n times into the array. Standard
     * memory rules prevent borrows from being stored in arrays. The @n argument is verified to be a const variable if
     * it is symbolic.
     * @param[in] sm The scope manager to use for type checking.
     * @param[in,out] meta Associated metadata.
     */
    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Check the memory of the elements inside the array literal. This is done by checking calling the same method on
     * each element.
     * @param sm The scope manager to use for memory checking.
     * @param meta Associated metadata.
     */
    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Create an array type based on the internal element type and the number of elements.
     * @param sm The scope manager to use for code generation.
     * @param meta Associated metadata.
     * @param ctx The LLVM context to use for code generation.
     * @return The LLVM value representing the array literal.
     */
    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    /**
     * The inferred type of an array literal is always @code std::array::Arr[T, n]@endcode, where @c T is the type of
     * the element in the array literal, and @c n is the size given.
     * @param [in] sm The scope manager to use for type inference.
     * @param [in,out] meta Associated metadata.
     * @return The @code std::array::Arr[T, n]@endcode type of the array literal.
     */
    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


spp::asts::ArrayLiteralRepeatedElementAst::~ArrayLiteralRepeatedElementAst() = default;
