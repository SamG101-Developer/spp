module;
#include <spp/macros.hpp>

export module spp.utils.errors;
import std;


namespace spp::utils::errors {
    SPP_EXP_CLS struct ErrorFormatter;
    SPP_EXP_CLS struct AbstractError;
    SPP_EXP_CLS template <typename T>
    struct AbstractErrorBuilder;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct Scope;
}


SPP_EXP_CLS struct spp::utils::errors::AbstractError : std::runtime_error {
    std::vector<std::string> messages;
    std::string final_message;

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
    std::unique_ptr<T> m_err_obj;
    std::vector<ErrorFormatter*> m_error_formatters;

public:
    template <typename... Args> requires std::is_constructible_v<T, Args...>
    auto with_args(Args &&... args) -> AbstractErrorBuilder&;

    auto with_scopes(std::vector<analyse::scopes::Scope const*> scopes) -> AbstractErrorBuilder&;

    auto with_error_formatter(ErrorFormatter *error_formatter) -> AbstractErrorBuilder&;

    SPP_ATTR_NORETURN virtual auto raise() -> void;
};
