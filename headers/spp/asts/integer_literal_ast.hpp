#ifndef INTEGER_LITERAL_AST_HPP
#define INTEGER_LITERAL_AST_HPP

#include <map>

#include <spp/asts/literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


constexpr auto INT_TYPE_MAP = std::map<icu::UnicodeString, std::unique_ptr<spp::asts::TypeAst>>{
};


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
     * The type of the integer literal. This is used to determine the type of the integer literal, such as @c I32,
     * @c U64, @c F16, etc. If not provided, the type is defaulted to @c BigInt, which is a large integer type that can
     * handle arbitrarily large integers.
     */
    std::unique_ptr<TypeAst> type;

    /**
     * Construct the IntegerLiteralAst with the arguments matching the members.
     * @param[in] sign The optionally provided sign token.
     * @param[in] tok_integer The token that represents the integer literal.
     * @param[in] type The type of the integer literal.
     */
    IntegerLiteralAst(
        decltype(sign) &&sign,
        decltype(tok_integer) &&tok_integer,
        icu::UnicodeString &&type);
};


#endif //INTEGER_LITERAL_AST_HPP
