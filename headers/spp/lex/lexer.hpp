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
    explicit Lexer(icu::UnicodeString &&code);
    auto lex() const -> std::vector<RawToken>;

private:
    icu::UnicodeString m_code;
};


#endif //LEXER_HPP
