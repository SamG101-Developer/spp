module spp.parse.errors.parser_error_builder;
import genex;


template <typename T>
auto spp::parse::errors::SyntacticErrorBuilder<T>::raise() -> void {
    using namespace std::string_literals;
    const auto cast_error = dynamic_cast<SyntacticError*>(this->m_err_obj.get());

    auto token_set_str = tokens
        | genex::views::transform(&lex::tok_to_string)
        | genex::views::intersperse(", "s)
        | genex::views::join
        | genex::to<std::string>();;
    token_set_str = "'" + token_set_str + "'";

    // Replace the "£" with the string tokens.
    auto err_msg = cast_error->header;
    err_msg.replace(err_msg.find("£"), 1, std::move(token_set_str));
    err_msg = this->m_error_formatters[0]->error_raw_pos(pos, 1, std::move(err_msg), "Syntax error");
    std::cout << err_msg << std::endl;
    this->m_err_obj->messages = {err_msg};
    utils::errors::AbstractErrorBuilder<T>::raise();
}
