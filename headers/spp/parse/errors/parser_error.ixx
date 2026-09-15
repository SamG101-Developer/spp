module;
#include <spp/macros.hpp>

export module spp.parse.errors.parser_error;
import spp.utils.errors;
import spp.utils.types;
import std;

use(spp::parse::errors, struct SyntacticError);
use(spp::parse::errors, struct SppSyntaxError);

/// The base syntactic error for the parser to use, currently
/// only inherited by the standard syntax error, but flexible
/// for future expansions.
SPP_EXP_CLS struct spp::parse::errors::SyntacticError :
  utils::errors::AbstractError {
  /// Header text.
  Str header;

  explicit SyntacticError(Str &&header);
  SyntacticError(SyntacticError const &) = default;
  ~SyntacticError() override = default;
};

/// Raised when there is invalid syntax being parsed. This
/// reports the incorrect token, and the allowed tokens to
/// follow it.
SPP_EXP_CLS struct spp::parse::errors::SppSyntaxError final :
  SyntacticError {
  explicit SppSyntaxError(Str &&header);
  ~SppSyntaxError() override = default;
};
