#pragma once
#include <spp/pch.hpp>
#include <spp/lex/tokens.hpp>


namespace spp::lex {
    class Lexer;
}


class spp::lex::Lexer {
public:
    explicit Lexer(std::string code);
    auto lex() const -> std::vector<RawToken>;

private:
    std::string m_code;
};
