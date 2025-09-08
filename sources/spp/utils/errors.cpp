// #include <spp/analyse/scopes/scope.hpp>
// #include <spp/utils/errors.hpp>
// #include <spp/utils/error_formatter.hpp>
//
// #include <genex/views/cycle.hpp>
// #include <genex/views/take.hpp>
// #include <genex/views/transform.hpp>
// #include <genex/views/zip.hpp>
//
// #include <magic_enum/magic_enum.hpp>
//
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
