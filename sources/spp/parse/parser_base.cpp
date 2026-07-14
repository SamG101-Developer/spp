module;
#include <spp/macros.hpp>

module spp.parse.parser_base;
import spp.lex.tokens;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;

SPP_MOD_BEGIN
spp::parse::ParserBase::ParserBase(Vec<lex::RawToken> tokens, Unique<utils::errors::ErrorFormatter> &&error_formatter) :
    _Tokens(std::move(tokens)),
    _TokensLen(_Tokens.Len()),
    _ErrorBuilder(MakeUnique<errors::SyntacticErrorBuilder<errors::SppSyntaxError>>()),
    _ErrorFormatter(error_formatter != nullptr ? std::move(error_formatter) : MakeUnique<utils::errors::ErrorFormatter>(_Tokens, "<temp>")) {
    _ErrorBuilder->WithErrorFormatter(_ErrorFormatter.Get());
}

spp::parse::ParserBase::~ParserBase() = default;
SPP_MOD_END
