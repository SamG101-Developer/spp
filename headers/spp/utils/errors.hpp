#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <exception>
#include <memory>
#include <set>

#include <spp/lex/tokens.hpp>
#include <spp/macros.hpp>


namespace spp::utils::errors {
    struct CustomError;
    struct SyntacticError;
    struct SemanticError;
    class ErrorFormatter;
}


struct spp::utils::errors::CustomError : std::runtime_error {
public:
    using std::runtime_error::runtime_error;
    virtual auto raise() noexcept(false) -> void;

protected:
    std::unique_ptr<ErrorFormatter> m_error_formatter;
};


struct spp::utils::errors::SyntacticError : CustomError {
public:
    using CustomError::CustomError;
    ~SyntacticError() override = default;
    auto raise() noexcept(false) -> void override;

public:
    std::size_t pos = 0;
    std::set<lex::SppTokenType> tokens = {};
};


struct spp::utils::errors::SemanticError : CustomError {
    using CustomError::CustomError;
    ~SemanticError() override = default;
};


#endif //ERRORS_HPP
