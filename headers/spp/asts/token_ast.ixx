module;
#include <magic_enum/magic_enum.hpp>
#include <spp/macros.hpp>

export module spp.asts.token_ast;
import spp.asts.ast;
import spp.lex.tokens;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TokenAst;
}


/**
 * A TokenAST represents a low level token created by the lexer. This includes @c = or @c + for example, seen in
 * statements and expressions. Associated token data is needed when strings or numbers are created for example. This AST
 * is also the terminator for @c pos_end() recursive calls; the end position is the length of the associated data added
 * to is the sart position.
 */
SPP_EXP_CLS struct spp::asts::TokenAst final : Ast {
    lex::SppTokenType token_type;
    std::string token_data;

    explicit TokenAst(
        lex::SppTokenType token_type,
        std::string &&token_data);
    ~TokenAst() override;
    auto to_rust() const -> std::string override;
};
