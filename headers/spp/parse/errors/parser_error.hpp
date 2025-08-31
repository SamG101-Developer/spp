#pragma once
#include <spp/utils/errors.hpp>


namespace spp::parse::errors {
    struct SyntacticError;

    struct SppSyntaxError;
}


struct spp::parse::errors::SyntacticError : utils::errors::AbstractError {
    using AbstractError::AbstractError;
    ~SyntacticError() override = default;
};


struct spp::parse::errors::SppSyntaxError final : SyntacticError {
    using SyntacticError::SyntacticError;
};
