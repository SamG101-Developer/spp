#pragma once
#include <set>
#include <spp/utils/errors.hpp>
#include <spp/lex/tokens.hpp>


namespace spp::parse {
    class ParserSpp;
}


namespace spp::parse::errors {
    template <typename T>
    struct SyntacticErrorBuilder;
}


template <typename T>
struct spp::parse::errors::SyntacticErrorBuilder : spp::utils::errors::AbstractErrorBuilder<T> {
    friend class spp::parse::ParserSpp;

private:
    std::size_t pos = 0;
    std::set<lex::SppTokenType> tokens = {};

public:
    SyntacticErrorBuilder() = default;
};
