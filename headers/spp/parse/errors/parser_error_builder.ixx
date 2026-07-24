module;
#include <spp/macros.hpp>

export module spp.parse.errors.parser_error_builder;
import spp.lex.tokens;
import spp.utils.errors;
import spp.utils.error_formatter;
import spp.utils.types;
import std;

namespace spp::parse::errors {
    SPP_EXP_CLS template <typename T>
    struct SyntacticErrorBuilder;
}

namespace spp::parse {
    SPP_EXP_CLS class ParserSpp;
}

SPP_EXP_CLS template <typename T>
struct SPP_ATTR_COLD spp::parse::errors::SyntacticErrorBuilder final : utils::errors::AbstractErrorBuilder<T> {
    std::size_t Pos = 0;

    std::set<lex::SppTokenType> Tokens = {};

    SyntacticErrorBuilder() = default;

    ~SyntacticErrorBuilder() override = default;

    SPP_ATTR_COLD SPP_ATTR_NORETURN auto Raise() -> void override {
        using namespace std::string_literals;

        // auto token_set_str = tokens
        //     | genex::views::transform(&lex::tok_to_string)
        //     | genex::views::intersperse(", "s)
        //     | genex::views::join
        //     | genex::to<Str>();
        auto token_set_str = Str();
        token_set_str = "'" + token_set_str + "'";

        // Replace the "£" with the string tokens.
        auto err_msg = this->_ErrObj->header;
        err_msg.replace(err_msg.find("£"), 1, std::move(token_set_str));
        err_msg = this->_ErrFormatters[0]->ErrorRawPos(Pos, 1, std::move(err_msg), "Syntax error");
        std::cout << err_msg << std::endl;
        this->_ErrObj->messages = {err_msg};

        utils::errors::AbstractErrorBuilder<T>::Raise();
    }
};
