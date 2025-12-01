module;
#include <spp/macros.hpp>

export module spp.asts.boolean_literal_ast;
import spp.asts.literal_ast;
import spp.asts.primary_expression_ast;
import spp.asts.token_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The BooleanLiteralAst represents a boolean literal in the source code, which can be either @c true or @c false.
 * This AST is used to represent boolean values in expressions and statements.
 */
SPP_EXP_CLS struct spp::asts::BooleanLiteralAst final : LiteralAst {
    /**
     * The token that represents the boolean literal, either @c true or @code false@endcode.
     */
    std::unique_ptr<TokenAst> tok_bool;

    /**
     * Construct the BooleanLiteralAst with the arguments matching the members.
     * @param[in] tok_bool The token that represents the boolean literal.
     */
    explicit BooleanLiteralAst(
        decltype(tok_bool) &&tok_bool);

    ~BooleanLiteralAst() override;

protected:
    SPP_ATTR_NODISCARD auto equals_boolean_literal(BooleanLiteralAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    /**
     * A static constructor to create a @c BooleanLiteralAst with a @c true value.
     * @param pos The position to create this AST at.
     * @return The created @c BooleanLiteralAst as a unique pointer.
     */
    static auto True(std::size_t pos) -> std::unique_ptr<BooleanLiteralAst>;

    /**
     * A static constructor to create a @c BooleanLiteralAst with a @c false value.
     * @param pos The position to create this AST at.
     * @return The created @c BooleanLiteralAst as a unique pointer.
     */
    static auto False(std::size_t pos) -> std::unique_ptr<BooleanLiteralAst>;

    /**
     * Generate the LLVM IR code for the boolean literal. This will produce an LLVM constant integer value of 1 for
     * @c true and 0 for @c false.
     * @param sm The scope manager to use for code generation.
     * @param meta Associated metadata.
     * @param ctx The LLVM context to generate code in.
     * @return The generated LLVM value representing the boolean literal.
     */
    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    /**
     * The boolean literal's type is always @c std::boolean::Bool, the compiler known type that represents a boolean
     * value.
     * @param sm The scope manager to use for type checking.
     * @param meta Associated metadata.
     * @return The standard boolean type @code std::boolean::Bool@endcode.
     */
    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


spp::asts::BooleanLiteralAst::~BooleanLiteralAst() = default;
