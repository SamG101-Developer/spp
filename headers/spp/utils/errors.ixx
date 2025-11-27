module;
#include <spp/macros.hpp>

#include <genex/to_container.hpp>
#include <genex/views/cycle.hpp>
#include <genex/views/join_with.hpp>
#include <genex/views/transform.hpp>

export module spp.utils.errors;
import spp.analyse.scopes.scope;
import spp.utils.error_formatter;


namespace spp::utils::errors {
    SPP_EXP struct AbstractError;

    SPP_EXP template <typename T>
    struct AbstractErrorBuilder;
}


SPP_EXP struct spp::utils::errors::AbstractError : std::runtime_error {
public:
    AbstractError() : std::runtime_error("") {
    }

    std::vector<std::string> messages;
    std::string final_message;

public:
    AbstractError(AbstractError const &) = default;
    ~AbstractError() override = default;

    [[nodiscard]]
    auto what() const noexcept -> const char* override {
        return final_message.c_str();
    }
};


SPP_EXP template <typename T>
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
        m_error_formatters = scopes | genex::views::transform(&analyse::scopes::Scope::get_error_formatter) | genex::to<std::vector>();
        return *this;
    }

    auto with_error_formatter(ErrorFormatter *error_formatter) -> AbstractErrorBuilder& {
        // Add a single error formatter to the list.
        m_error_formatters.emplace_back(error_formatter);
        return *this;
    }

    SPP_ATTR_NORETURN virtual auto raise() -> void {
        // Throw the error object.
        this->m_err_obj->final_message = this->m_err_obj->messages
            | genex::views::join_with('\n')
            | genex::to<std::string>();

        throw T(*m_err_obj);
    }
};
