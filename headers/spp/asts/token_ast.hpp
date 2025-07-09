#ifndef TOKEN_AST_HPP
#define TOKEN_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>
#include <spp/lex/tokens.hpp>

#include <magic_enum/magic_enum.hpp>


/**
 * A TokenAST represents a low level token created by the lexer. This includes \c = or \c + for example, seen in
 * statements and expressions. Associated token data is needed when strings or numbers are created for example. This AST
 * is also the terminator for \c pos_end() recursive calls; the end position is the length of the associated data added
 * to is the sart position.
 */
struct spp::asts::TokenAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token type (part of the enum) that this AST is wrapping.
     */
    lex::SppTokenType token_type;

    /**
     * The associated data to that token (normally the string representation).
     */
    icu::UnicodeString token_data;

    TokenAst(
        std::size_t pos,
        lex::SppTokenType token_type,
        icu::UnicodeString &&token_data);

    /**
     * Two tokens are equal if their token types are equal.
     * @param[in] that The other TokenAst to compare with.
     * @return Whether the two ASTs are equal or not.
     */
    auto operator==(TokenAst const &that) const -> bool;

private:
    std::size_t m_pos;
};


template <>
struct std::hash<spp::asts::TokenAst> {
    /**
     * To make a TokenAst hashable, the token type's string-converted name is used as the key, which is then used by
     * \c std::hash<std::string> or a primitive equivalent.
     * @param[in] t The TokenAst to hash.
     * @return The numeric mapped hash value.
     */
    auto operator()(spp::asts::TokenAst const &t) const noexcept -> std::size_t {
        return std::hash<std::string>()(magic_enum::enum_name(t.token_type).data());
    }
};


#endif //TOKEN_AST_HPP
