#ifndef INTEGER_LITERAL_AST_HPP
#define INTEGER_LITERAL_AST_HPP

#include <spp/asts/literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IntegerLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optionally provided sign token. This can be either a @c + or @c - sign, indicating the sign of the integer
     * literal. No sign means the integer is positive by default.
     */
    std::unique_ptr<TokenAst> sign;

    /**
     * The token that represents the integer literal. This is the actual integer value in the source code.
     */
    std::unique_ptr<TokenAst> tok_integer;

    /**
     * The raw type of the integer literal. This is from the postfix tag to the literal, like "i32" or "u64".
     */
    std::string raw_type;

    /**
     * The type of the integer literal. This is the type that the integer literal will be converted to, like @c i32 or
     * @c u64.
     */
    std::unique_ptr<TypeAst> true_type;

    /**
     * Construct the IntegerLiteralAst with the arguments matching the members.
     * @param[in] sign The optionally provided sign token.
     * @param[in] tok_integer The token that represents the integer literal.
     * @param[in] type The type of the integer literal.
     */
    IntegerLiteralAst(
        decltype(sign) &&sign,
        decltype(tok_integer) &&tok_integer,
        std::string &&type);

    ~IntegerLiteralAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //INTEGER_LITERAL_AST_HPP
