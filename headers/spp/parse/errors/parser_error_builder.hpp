#pragma once
#include <set>
#include <spp/utils/errors.hpp>
#include <spp/lex/tokens.hpp>
#include <spp/parse/errors/parser_error.hpp>

#include <genex/views/transform.hpp>
#include <genex/views/intersperse.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/to.hpp>

/// @cond
namespace spp::parse {
    class ParserSpp;
}

namespace spp::parse::errors {
    template <typename T>
    struct SyntacticErrorBuilder;
}
/// @endcond


template <typename T>
struct spp::parse::errors::SyntacticErrorBuilder final : spp::utils::errors::AbstractErrorBuilder<T> {
    friend class spp::parse::ParserSpp;

private:
    std::size_t pos = 0;
    std::set<lex::SppTokenType> tokens = {};

public:
    SyntacticErrorBuilder() = default;

    [[noreturn]]
    auto raise() -> void override {
        using namespace std::string_literals;
        const auto cast_error = dynamic_cast<SyntacticError*>(this->m_err_obj.get());

        auto token_set_str = tokens
            | genex::views::transform(&lex::tok_to_string)
            | genex::views::intersperse(", "s)
            | genex::views::flatten
            | genex::views::to<std::string>();;
        token_set_str = "'" + token_set_str + "'";

        // Replace the "£" with the string tokens.
        auto err_msg = cast_error->header;
        err_msg.replace(err_msg.find("£"), 1, std::move(token_set_str));
        err_msg = this->m_error_formatters[0]->error_raw_pos(pos, 1, std::move(err_msg), "Syntax error");
        std::cout << err_msg << std::endl;
        this->m_err_obj->messages = {err_msg};
        utils::errors::AbstractErrorBuilder<T>::raise();
    }
};
