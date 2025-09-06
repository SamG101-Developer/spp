#include <ranges>

#include <spp/analyse/scopes/scope.hpp>
#include <spp/utils/errors.hpp>
#include <spp/utils/error_formatter.hpp>

#include <genex/views/cycle.hpp>
#include <genex/views/take.hpp>
#include <genex/views/transform.hpp>
#include <genex/views/zip.hpp>

#include <magic_enum/magic_enum.hpp>


template <typename T>
template <typename... Args> requires std::is_constructible_v<T, Args...>
auto spp::utils::errors::AbstractErrorBuilder<T>::with_args(
    Args &&... args)
    -> AbstractErrorBuilder& {
    // Provide the arguments to construct the error object.
    m_err_obj = std::make_unique<T>(std::forward<Args>(args)...);
    return *this;
}


template <typename T>
auto spp::utils::errors::AbstractErrorBuilder<T>::with_scopes(
    std::vector<analyse::scopes::Scope const*> scopes)
    -> AbstractErrorBuilder& {
    // Extract error formatters from a list of scopes.
    m_error_formatters = scopes | genex::views::transform(&analyse::scopes::Scope::get_error_formatter) | genex::views::to<std::vector>();
    return *this;
}


template <typename T>
auto spp::utils::errors::AbstractErrorBuilder<T>::with_error_formatter(
    ErrorFormatter *error_formatter)
    -> AbstractErrorBuilder& {
    // Add a single error formatter to the list.
    m_error_formatters.push_back(error_formatter);
    return *this;
}


template <typename T>
auto spp::utils::errors::AbstractErrorBuilder<T>::raise()
    -> void {
    // Cycle the formatters to match the number of strings being formatted.
    auto formatters = m_error_formatters
        | genex::views::cycle
        | genex::views::take(m_err_obj->messages.size())
        | genex::views::to<std::vector>();

    // Format all the error strings by the correct formatter (file agnostic).
    m_err_obj->messages = m_err_obj->messages
        | genex::views::zip(std::move(formatters))
        | genex::views::transform([](auto &&x) { return x.first->format(std::move(x.second)); })
        | genex::views::to<std::vector>();

    // Throw the error object.
    throw T(*m_err_obj);
}


// // Todo: move to parse::error -> error builder
// auto spp::utils::errors::SyntacticError::get_message(ErrorFormatter &error_formatter) const noexcept -> std::string {
//     // Create a set of the tokens to eliminate duplicates.
//     // auto token_set_str = tokens
//
//     auto token_set_str = std::views::join_with(tokens | std::views::transform([](const lex::SppTokenType token) {
//         return lex::tok_to_string(token);
//     }), ", ") | std::ranges::to<std::string>();
//     token_set_str = "'" + token_set_str + "'";
//
//     // Replace the "£" with the string tokens.
//     auto err_msg = std::string(what());
//     err_msg.replace(err_msg.find("£"), 1, token_set_str);
//     err_msg = error_formatter.error_raw_pos(pos, pos + 1, std::move(err_msg), "Syntax error");
//     return err_msg;
// }
//
//
// auto spp::utils::errors::SyntacticError::raise(ErrorFormatter &error_formatter) noexcept(false) -> void {
//     // Get the error message and throw it.
//     const auto err_msg = get_message(error_formatter);
//     throw SyntacticError(err_msg.c_str());
// }
