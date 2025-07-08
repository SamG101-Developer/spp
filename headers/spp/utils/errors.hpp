#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <exception>


namespace spp::utils::errors {
    struct SyntacticError;
    struct SemanticError;
}


struct spp::utils::errors::SyntacticError : std::exception {
    ~SyntacticError() override = default;
};


struct spp::utils::errors::SemanticError: std::exception {
    ~SemanticError() override = default;
};


#endif //ERRORS_HPP
