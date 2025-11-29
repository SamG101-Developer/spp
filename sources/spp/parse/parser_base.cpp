module spp.parse.parser_base;
import spp.lex.tokens;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;


spp::parse::ParserBase::ParserBase(std::vector<lex::RawToken> tokens, std::shared_ptr<utils::errors::ErrorFormatter> const &error_formatter) :
    m_tokens(std::move(tokens)),
    m_tokens_len(m_tokens.size()),
    m_error_builder(std::make_unique<errors::SyntacticErrorBuilder<errors::SppSyntaxError>>()),
    m_error_formatter(error_formatter ? error_formatter : std::make_shared<utils::errors::ErrorFormatter>(m_tokens, "<temp>")) {
    m_error_builder->with_error_formatter(m_error_formatter.get());
}


spp::parse::ParserBase::~ParserBase() = default;
