#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <exception>
#include <memory>
#include <vector>
#include <genex/views/flatten.hpp>

// #include <spp/analyse/scopes/scope.hpp>

#include <genex/views/cycle.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/map.hpp>
#include <genex/views/take.hpp>
#include <genex/views/to.hpp>
#include <genex/views/zip.hpp>


namespace spp::utils::errors {
    class ErrorFormatter;

    struct AbstractError;

    template <typename T>
    struct AbstractErrorBuilder;
}

namespace spp::analyse::scopes {
    class Scope;
}


struct spp::utils::errors::AbstractError : std::runtime_error {
public:
    std::vector<std::string> messages;

public:
    using std::runtime_error::runtime_error;
    AbstractError(AbstractError const &) noexcept = default;
    ~AbstractError() override = default;

    [[nodiscard]]
    auto what() const noexcept -> const char* override {
        const auto joined = messages
            | genex::views::flatten_with('\n')
            | genex::views::to<std::string>();
        return joined.c_str();
    }
};


template <typename T>
struct spp::utils::errors::AbstractErrorBuilder {
    AbstractErrorBuilder() = default;

private:
    std::unique_ptr<AbstractError> m_err_obj;
    std::vector<ErrorFormatter*> m_error_formatters;

public:
    template <typename... Args> requires std::is_constructible_v<T, Args...>
    auto with_args(Args &&... args) -> AbstractErrorBuilder&;

    auto with_scopes(std::vector<analyse::scopes::Scope*> scopes) -> AbstractErrorBuilder&;

    auto with_error_formatter(ErrorFormatter *error_formatter) -> AbstractErrorBuilder&;

    [[noreturn]] auto raise() -> void;
};


#endif //ERRORS_HPP
