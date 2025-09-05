#pragma once
#include <spp/asts/literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The BooleanLiteralAst represents a boolean literal in the source code, which can be either @c true or @c false.
 * This AST is used to represent boolean values in expressions and statements.
 */
struct spp::asts::BooleanLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

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
     * The boolean literal's type is always @c std::boolean::Bool, the compiler known type that represents a boolean
     * value.
     * @param sm The scope manager to use for type checking.
     * @param meta Associated metadata.
     * @return The standard boolean type @code std::boolean::Bool@endcode.
     */
    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
