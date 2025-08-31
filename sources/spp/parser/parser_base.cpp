#include <spp/parse/parser_base.hpp>

#include <spp/utils/error_formatter.hpp>
#include <spp/utils/errors.hpp>


spp::parse::ParserBase::ParserBase(std::vector<lex::RawToken> tokens, std::string_view file_name, std::unique_ptr<utils::errors::ErrorFormatter> error_formatter) :
    m_tokens(std::move(tokens)),
    m_tokens_len(m_tokens.size()),
    m_error_builder(std::make_unique<errors::SyntacticErrorBuilder<errors::SppSyntaxError>>()),
    m_error_formatter(error_formatter != nullptr ? std::move(error_formatter) : std::make_unique<utils::errors::ErrorFormatter>(m_tokens, file_name)) {
    m_error_builder->with_error_formatter(m_error_formatter.get());
}
