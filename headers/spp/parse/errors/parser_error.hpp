#pragma once
#include <set>
#include <spp/utils/errors.hpp>


/// @cond
namespace spp::parse::errors {
    struct SyntacticError;
    struct SppSyntaxError;
}

/// @endcond


struct spp::parse::errors::SyntacticError : utils::errors::AbstractError {
    ~SyntacticError() override = default;
    explicit SyntacticError(std::string &&header);
    SyntacticError(SyntacticError const &) = default;

    std::string header;
};


struct spp::parse::errors::SppSyntaxError final : SyntacticError {
    explicit SppSyntaxError(std::string &&header);

    std::size_t pos;
    std::set<lex::SppTokenType> tokens;
};
