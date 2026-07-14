module;
#include <spp/macros.hpp>

export module spp.utils.errors;
import spp.utils.error_formatter;
import spp.utils.types;
import std;

namespace spp::utils::errors {
    SPP_EXP_CLS struct AbstractError;
    SPP_EXP_CLS template <typename T>
    struct AbstractErrorBuilder;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

SPP_EXP_CLS struct spp::utils::errors::AbstractError : std::runtime_error {
    Vec<Str> messages;
    Str final_message;

    AbstractError() : std::runtime_error("") {
    }

    AbstractError(AbstractError const &) = default;
    ~AbstractError() override = default;

    [[nodiscard]]
    auto what() const noexcept -> const char* override {
        return final_message.c_str();
    }
};

SPP_EXP_CLS template <typename T>
struct spp::utils::errors::AbstractErrorBuilder {
    AbstractErrorBuilder() = default;
    virtual ~AbstractErrorBuilder() = default;

protected:
    Unique<T> _ErrObj;
    Vec<ErrorFormatter*> _ErrFormatters;

public:
    template <typename... Args> requires std::is_constructible_v<T, Args...>
    auto WithArgs(Args &&... args) -> AbstractErrorBuilder& {
        // Provide the arguments to construct the error object.
        _ErrObj = MakeUnique<T>(std::forward<Args>(args)...);
        return *this;
    }

    auto WithFormatters(Vec<ErrorFormatter*> const &formatters) -> AbstractErrorBuilder& {
        // Bind the error formatters to the builder.
        _ErrFormatters = formatters;
        return *this;
    }

    auto WithErrorFormatter(ErrorFormatter *error_formatter) -> AbstractErrorBuilder& {
        // Add a single error formatter to the list.
        _ErrFormatters.EmplaceBack(error_formatter);
        return *this;
    }

    SPP_ATTR_NORETURN virtual auto Raise() -> void {
        // Throw the error object.
        this->_ErrObj->final_message = this->_ErrObj->messages
            | std::views::join_with('\n')
            | std::ranges::to<Str>();
        throw T(*_ErrObj);
    }
};
