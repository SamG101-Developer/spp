#ifndef PARSER_ERRORS_HPP
#define PARSER_ERRORS_HPP

#include <spp/utils/errors.hpp>


namespace spp::parse::errors {
    struct SyntaxError;
    struct EoFError;
    struct ManyParseError;
    struct AlternateParseError;
}


struct spp::parse::errors::SyntaxError final : utils::errors::SyntacticError {
};


struct spp::parse::errors::EoFError final : utils::errors::SyntacticError {
};


struct spp::parse::errors::ManyParseError final : utils::errors::SyntacticError {
};


struct spp::parse::errors::AlternateParseError final : utils::errors::SyntacticError {
};


#endif //PARSER_ERRORS_HPP
