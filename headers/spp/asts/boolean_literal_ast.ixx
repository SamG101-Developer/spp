module;
#include <spp/macros.hpp>

export module spp.asts.boolean_literal_ast;
import spp.asts.literal_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

/**
 * The BooleanLiteralAst represents a boolean literal in the source code, which can be either @c true or @c false.
 * This AST is used to represent boolean values in expressions and statements.
 */
SPP_EXP_CLS struct spp::asts::BooleanLiteralAst final : LiteralAst {
    SPP_GCC_VTABLE_FIX
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the boolean literal, either @c true or @code false@endcode.
     */
    Unique<TokenAst> TokBool;

    /**
     * Construct the BooleanLiteralAst with the arguments matching the members.
     * @param[in] tok_bool The token that represents the boolean literal.
     */
    explicit BooleanLiteralAst(
        decltype(TokBool) &&tok_bool);

    ~BooleanLiteralAst() override;

    /**
     * Check for equality with another boolean literal. This will check if the other boolean literal has the same value
     * as this one, meaning they both represent either @c true or @c false.
     * @param other The other boolean literal to compare against.
     * @return @code Ordering::equal@endcode if the boolean literals are equal, and
     * @code Ordering::less@endcode otherwise.
     */
    SPP_ATTR_NODISCARD auto EqualsBooleanLiteral(BooleanLiteralAst const &other) const -> Ordering override;

    /**
     * A reverse hook to equate against the other arguments. This will call the @c equals_boolean_literal method on the
     * other expression, if it is a boolean literal, to check for equality.
     * @param other The other expression to compare against.
     * @return @code Ordering::equal@endcode if the boolean literals are equal, and
     * @code Ordering::less@endcode otherwise.
     */
    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    /**
     * A static constructor to create a @c BooleanLiteralAst with a @c true value.
     * @param pos The position to create this AST at.
     * @return The created @c BooleanLiteralAst as a unique pointer.
     */
    static auto True(std::size_t pos) -> Unique<BooleanLiteralAst>;

    /**
     * A static constructor to create a @c BooleanLiteralAst with a @c false value.
     * @param pos The position to create this AST at.
     * @return The created @c BooleanLiteralAst as a unique pointer.
     */
    static auto False(std::size_t pos) -> Unique<BooleanLiteralAst>;

    /**
     * Check if this boolean literal represents the @c true value. This will return @c true if the internally stored
     * token represents the @c true literal, and @c false if it represents the @c false literal.
     * @return @c true if this boolean literal represents the @c true value, and @c false if it represents the @c false
     * value.
     */
    SPP_ATTR_NODISCARD auto IsTrue() const -> bool;

    /**
     * Extract the internally stored token into a boolean value. This will return @c true if the token represents the
     * @c true literal, and @c false if it represents the @c false literal.
     * @return The c++ boolean value represented by this boolean literal AST.
     */
    SPP_ATTR_NODISCARD auto CppVal() const -> bool;

    /**
     * Resolve the boolean literal at compile time. This will produce a compile time value representing either @c true
     * or @c false.
     * @param sm The scope manager to use for resolution.
     * @param meta Associated metadata.
     * @return The compile time resolved boolean literal.
     */
    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    /**
     * Generate the LLVM IR code for the boolean literal. This will produce an LLVM constant integer value of 1 for
     * @c true and 0 for @c false.
     * @param sm The scope manager to use for code generation.
     * @param meta Associated metadata.
     * @param ctx The LLVM context to generate code in.
     * @return The generated LLVM value representing the boolean literal.
     */
    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    /**
     * The boolean literal's type is always @c std::boolean::Bool, the compiler known type that represents a boolean
     * value.
     * @param sm The scope manager to use for type checking.
     * @param meta Associated metadata.
     * @return The standard boolean type @code std::boolean::Bool@endcode.
     */
    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::BooleanLiteralAst)
