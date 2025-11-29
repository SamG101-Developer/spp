module;
#include <spp/macros.hpp>

export module spp.parse.errors.parser_error_builder;
import spp.lex.tokens;
import spp.utils.errors;
import spp.utils.error_formatter;

import genex;
import std;

namespace spp::parse::errors {
    SPP_EXP_CLS template <typename T>
    struct SyntacticErrorBuilder;
}

namespace spp::parse {
    SPP_EXP_CLS class ParserSpp;
}


SPP_EXP_CLS template <typename T>
struct spp::parse::errors::SyntacticErrorBuilder final : utils::errors::AbstractErrorBuilder<T> {
    friend class spp::parse::ParserSpp;

private:
    std::size_t pos = 0;
    std::set<lex::SppTokenType> tokens = {};

public:
    SyntacticErrorBuilder() = default;

    SPP_ATTR_NORETURN auto raise() -> void override {
        using namespace std::string_literals;

        auto token_set_str = tokens
            | genex::views::transform(&lex::tok_to_string)
            | genex::views::intersperse(", "s)
            | genex::views::join
            | genex::to<std::string>();
        token_set_str = "'" + token_set_str + "'";

        // Replace the "£" with the string tokens.
        auto err_msg = this->m_err_obj->header;
        err_msg.replace(err_msg.find("£"), 1, std::move(token_set_str));
        err_msg = this->m_error_formatters[0]->error_raw_pos(pos, 1, std::move(err_msg), "Syntax error");
        std::cout << err_msg << std::endl;
        this->m_err_obj->messages = {err_msg};
        utils::errors::AbstractErrorBuilder<T>::raise();
    }
};
