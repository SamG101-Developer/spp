module;
#include <spp/macros.hpp>

export module spp.lex.lexer;
import spp.lex.tokens;

import std;


namespace spp::lex {
    SPP_EXP_CLS class Lexer;
}


/**
 * The Lexer is responsible for converting source code into a list of tokens. It takes a string of source code as input
 * and produces a vector of @c RawToken objects as output.
 */
SPP_EXP_CLS class spp::lex::Lexer {
    std::string m_code;

public:
    explicit Lexer(std::string code);
    auto lex() const -> std::vector<RawToken>;
};
