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
    /**
     * Very similar to the constructor, but requires for the macro'd unified "new_empty" caller for defaulting
     * attributes. The pos isn't wlawys given so is optional.
     * @param token_type The token type from the enumeration.
     * @param token_data The associated token data (stringified token, usually)
     * @param pos The optional position of this token.
     * @return The created token as a unique pointer.
     */
    static auto new_empty(
        lex::SppTokenType token_type,
        std::string &&token_data,
        std::size_t pos = 0)
        -> std::unique_ptr<TokenAst>;

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

    /**
     * Two tokens are equal if their token types are equal.
     * @param[in] that The other TokenAst to compare with.
     * @return Whether the two ASTs are equal or not.
     */
    auto operator==(TokenAst const &that) const -> bool;

private:
    std::size_t m_pos;
};
