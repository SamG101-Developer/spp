#include <spp/utils/error_formatter.hpp>
#include <spp/utils/errors.hpp>
#include <spp/parse/parser_base.hpp>


spp::parse::ParserBase::ParserBase(std::vector<lex::RawToken> tokens, utils::errors::ErrorFormatter *error_formatter) :
    m_tokens(std::move(tokens)),
    m_tokens_len(m_tokens.size()),
    m_error_builder(std::make_unique<errors::SyntacticErrorBuilder<errors::SppSyntaxError>>()),
    m_error_formatter(error_formatter) {
    m_error_builder->with_error_formatter(m_error_formatter);
}
