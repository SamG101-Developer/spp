module;
#include <spp/macros.hpp>

module spp.asts.token_ast;
import spp.lex.tokens;

SPP_MOD_BEGIN
auto spp::asts::TokenAst::NewEmpty(
    lex::SppTokenType token_type,
    Str &&token_data,
    const std::size_t pos)
    -> Unique<TokenAst> {
    return MakeUnique<TokenAst>(pos, token_type, std::move(token_data));
}

spp::asts::TokenAst::TokenAst(
    const std::size_t pos,
    const lex::SppTokenType token_type,
    Str &&token_data) :
    TokenType(token_type),
    TokenData(std::move(token_data)),
    _Pos(pos) {
}

spp::asts::TokenAst::~TokenAst() = default;

auto spp::asts::TokenAst::PosStart() const
    -> std::size_t {
    // Use the local position.
    return _Pos;
}

auto spp::asts::TokenAst::PosEnd() const
    -> std::size_t {
    // Keywords appear in the raw token stream as one multi-char raw token (e.g. "fun" is one
    // token, not three 'f'/'u'/'n' tokens), so the exclusive end index is _Pos + 1.
    // All other tokens use one raw token per source character, so data.length() gives the span.
    const auto is_single_raw_token_keyword =
        TokenType >= lex::SppTokenType::KW_CLS &&
        TokenType <= lex::SppTokenType::KW_CAPS;
    return is_single_raw_token_keyword ? _Pos + 1 : _Pos + TokenData.length();
}

auto spp::asts::TokenAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TokenAst>(_Pos, TokenType, TokenData.c_str());
}

auto spp::asts::TokenAst::ToString() const
    -> Str {
    // Use the token data.
    return TokenData;
}

auto spp::asts::TokenAst::operator==(
    TokenAst const &that) const
    -> bool {
    // Equality is dependent on the token type alone.
    return TokenType == that.TokenType;
}

auto spp::asts::TokenAst::PatchPos(const std::size_t pos) -> void {
    // Manually override the pos of this token.
    _Pos = pos;
}

SPP_MOD_END
