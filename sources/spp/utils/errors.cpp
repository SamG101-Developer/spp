module;
#include <genex/to_container.hpp>
#include <genex/views/join_with.hpp>
#include <genex/views/transform.hpp>

module spp.utils.errors;
import spp.analyse.scopes.scope;


template <typename T>
template <typename... Args> requires std::is_constructible_v<T, Args...>
auto spp::utils::errors::AbstractErrorBuilder<T>::with_args(Args &&... args) -> AbstractErrorBuilder& {
    // Provide the arguments to construct the error object.
    m_err_obj = std::make_unique<T>(std::forward<Args>(args)...);
    return *this;
}

template <typename T>
auto spp::utils::errors::AbstractErrorBuilder<T>::with_scopes(std::vector<analyse::scopes::Scope const*> scopes) -> AbstractErrorBuilder& {
    // Extract error formatters from a list of scopes.
    m_error_formatters = scopes | genex::views::transform(&analyse::scopes::Scope::get_error_formatter) | genex::to<std::vector>();
    return *this;
}


template <typename T>
auto spp::utils::errors::AbstractErrorBuilder<T>::with_error_formatter(ErrorFormatter *error_formatter) -> AbstractErrorBuilder& {
    // Add a single error formatter to the list.
    m_error_formatters.emplace_back(error_formatter);
    return *this;
}


template <typename T>
auto spp::utils::errors::AbstractErrorBuilder<T>::raise() -> void {
    // Throw the error object.
    this->m_err_obj->final_message = this->m_err_obj->messages
        | genex::views::join_with('\n')
        | genex::to<std::string>();
    throw T(*m_err_obj);
}
