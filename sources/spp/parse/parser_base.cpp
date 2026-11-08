module;
#include <spp/macros.hpp>

module spp.parse.parser_base;
import spp.lex.tokens;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::parse::errors, SppSyntaxError);
use_alias(spp::parse::errors, SyntacticErrorBuilder);

SPP_MOD_BEGIN
spp::parse::ParserBase::ParserBase(Vec<lex::RawToken> tokens,
  Shared<utils::errors::ErrorFormatter> const &error_formatter) :
  _Tokens(std::move(tokens)),
  _TokensLen(_Tokens.size()),
  _ErrorBuilder(MakeUnique<errors::SyntacticErrorBuilder<errors::SppSyntaxError>>()),
  _ErrorFormatter(error_formatter ? error_formatter : MakeShared<utils::errors::ErrorFormatter>(_Tokens, "<temp>")) {
  _ErrorBuilder->WithErrorFormatter(_ErrorFormatter.get());
}

spp::parse::ParserBase::~ParserBase() = default;
SPP_MOD_END
