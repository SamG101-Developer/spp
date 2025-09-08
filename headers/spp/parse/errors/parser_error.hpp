#pragma once
#include <spp/utils/errors.hpp>


namespace spp::parse::errors {
    struct SyntacticError;

    struct SppSyntaxError;
}


struct spp::parse::errors::SyntacticError : utils::errors::AbstractError {
    using AbstractError::AbstractError;
    ~SyntacticError() override = default;
    SyntacticError(SyntacticError const &) = default;
};


struct spp::parse::errors::SppSyntaxError final : SyntacticError {
    explicit SppSyntaxError(std::string &&);
};
