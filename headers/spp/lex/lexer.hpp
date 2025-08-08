#ifndef LEXER_HPP
#define LEXER_HPP

#include <vector>

#include <spp/lex/tokens.hpp>

#include <unicode/unistr.h>


namespace spp::lex {
    class Lexer;
}


class spp::lex::Lexer {
public:
    explicit Lexer(std::string &&code);
    auto lex() const -> std::vector<RawToken>;

private:
    std::string m_code;
};


#endif //LEXER_HPP
