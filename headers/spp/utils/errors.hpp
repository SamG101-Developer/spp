#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <exception>
#include <memory>
#include <set>
#include <vector>

#include <spp/lex/tokens.hpp>


namespace spp::utils::errors {
    struct CustomError;
    struct SyntacticError;
    struct SemanticError;
    class ErrorFormatter;
}

namespace spp::analyse::scopes {
    class Scope;
}


struct spp::utils::errors::CustomError : std::runtime_error {
public:
    using std::runtime_error::runtime_error;
    virtual auto get_message(ErrorFormatter &error_formatter) const noexcept -> std::string = 0;
};


struct spp::utils::errors::SyntacticError : CustomError {
public:
    using CustomError::CustomError;
    ~SyntacticError() override = default;
    auto get_message(ErrorFormatter &error_formatter) const noexcept -> std::string override;
    auto raise(ErrorFormatter &error_formatter) noexcept(false) -> void;

public:
    std::size_t pos = 0;
    std::set<lex::SppTokenType> tokens = {};
};


struct spp::utils::errors::SemanticError : CustomError {
    using CustomError::CustomError;
    ~SemanticError() override = default;
    auto get_message(ErrorFormatter &error_formatter) const noexcept -> std::string override;
    auto raise() noexcept(false) -> void;
    auto scopes(std::vector<analyse::scopes::Scope const*> &&scopes) -> SemanticError&;
};


#endif //ERRORS_HPP
