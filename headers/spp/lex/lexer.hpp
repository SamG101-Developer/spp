#pragma once
#include <spp/pch.hpp>
#include <spp/lex/tokens.hpp>


/// @cond
namespace spp::lex {
    class Lexer;
}
/// @endcond


/**
 * The Lexer is responsible for converting source code into a list of tokens. It takes a string of source code as input
 * and produces a vector of @c RawToken objects as output.
 */
class spp::lex::Lexer {
public:
    explicit Lexer(std::string code);
    auto lex() const -> std::vector<RawToken>;

private:
    std::string m_code;
};
