module;
#include <spp/macros.hpp>

module spp.parse.errors.parser_error;

SPP_MOD_BEGIN
spp::parse::errors::SyntacticError::SyntacticError(Str &&header) :
  header(std::move(header)) {
}

spp::parse::errors::SppSyntaxError::SppSyntaxError(Str &&header) :
  SyntacticError(std::move(header)) {
}

SPP_MOD_END
