module;
#include <magic_enum/magic_enum.hpp>
#include <spp/macros.hpp>

export module spp.asts.token_ast;
import spp.asts.ast;
import spp.lex.tokens;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

/**
 * A TokenAST represents a low level token created by the lexer. This includes @c = or @c + for example, seen in
 * statements and expressions. Associated token data is needed when strings or numbers are created for example. This AST
 * is also the terminator for @c pos_end() recursive calls; the end position is the length of the associated data added
 * to is the sart position.
 */
SPP_EXP_CLS struct spp::asts::TokenAst final : virtual Ast {
    friend struct spp::asts::TypeIdentifierAst;

    /**
     * The token type (part of the enum) that this AST is wrapping.
     */
    lex::SppTokenType token_type;

    /**
     * The associated data to that token (normally the string representation).
     */
    std::string token_data;

    TokenAst(
        std::size_t pos,
        lex::SppTokenType token_type,
        std::string &&token_data);

    ~TokenAst() override;

    SPP_AST_KEY_FUNCTIONS;

    static auto new_empty(
        lex::SppTokenType token_type,
        std::string &&token_data,
        std::size_t pos = 0)
        -> std::unique_ptr<TokenAst>;

    /**
     * Two tokens are equal if their token types are equal.
     * @param[in] that The other TokenAst to compare with.
     * @return Whether the two ASTs are equal or not.
     */
    auto operator==(TokenAst const &that) const -> bool; // todo: do as friend

private:
    std::size_t m_pos;
};


template <>
struct std::hash<spp::asts::TokenAst> {
    /**
     * To make a TokenAst hashable, the token type's string-converted name is used as the key, which is then used by
     * @c std::hash<std::string> or a primitive equivalent.
     * @param[in] t The TokenAst to hash.
     * @return The numeric mapped hash value.
     */
    auto operator()(spp::asts::TokenAst const &t) const noexcept
        -> std::size_t {
        using UT = std::underlying_type_t<spp::lex::SppTokenType>;
        return std::hash<UT>()(std::bit_cast<UT>(t.token_type));
    }
};
