#pragma once
#include <spp/analyse/scopes/scope.hpp>
#include <spp/pch.hpp>

#include <genex/views/flatten_with.hpp>
#include <genex/views/to.hpp>
#include <genex/views/cycle.hpp>
#include <genex/views/take.hpp>
#include <genex/views/transform.hpp>
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
    AbstractError() : std::runtime_error("") {
    }

    std::vector<std::string> messages;

public:
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
    virtual ~AbstractErrorBuilder() = default;

protected:
    std::unique_ptr<T> m_err_obj;
    std::vector<ErrorFormatter*> m_error_formatters;

public:
    template <typename... Args> requires std::is_constructible_v<T, Args...>
    auto with_args(Args &&... args) -> AbstractErrorBuilder& {
        // Provide the arguments to construct the error object.
        m_err_obj = std::make_unique<T>(std::forward<Args>(args)...);
        return *this;
    }

    auto with_scopes(std::vector<analyse::scopes::Scope const*> scopes) -> AbstractErrorBuilder& {
        // Extract error formatters from a list of scopes.
        m_error_formatters = scopes | genex::views::transform(&analyse::scopes::Scope::get_error_formatter) | genex::views::to<std::vector>();
        return *this;
    }

    auto with_error_formatter(ErrorFormatter *error_formatter) -> AbstractErrorBuilder& {
        // Add a single error formatter to the list.
        m_error_formatters.push_back(error_formatter);
        return *this;
    }

    [[noreturn]] virtual auto raise() -> void {
        // Throw the error object.
        throw T(*m_err_obj);
    }
};
