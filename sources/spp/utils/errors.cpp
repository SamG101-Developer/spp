#include <ranges>

#include <spp/utils/errors.hpp>
#include <spp/utils/error_formatter.hpp>

#include <magic_enum/magic_enum.hpp>


auto spp::utils::errors::SyntacticError::raise() noexcept(false) -> void {
    // Create a set of the tokens to eliminate duplicates.
    auto token_set_str = std::views::join_with(tokens | std::views::transform([](const lex::SppTokenType &token) {
        return magic_enum::enum_name(token);
    }), ", ") | std::ranges::to<std::string>();
    token_set_str = "'" + token_set_str + "'";

    // Replace the "£" with the string tokens.
    auto err_msg = std::string(what());
    err_msg.replace(err_msg.find("£"), 1, token_set_str);
    err_msg = m_error_formatter->error_from_positions(pos, std::move(err_msg), "Syntax error");
    throw SyntacticError(err_msg.c_str());
}
