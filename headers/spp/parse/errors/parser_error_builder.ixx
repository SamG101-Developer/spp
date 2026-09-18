module;
#include <spp/macros.hpp>

export module spp.parse.errors.parser_error_builder;
import spp.lex.tokens;
import spp.utils.error_formatter;
import spp.utils.errors;
import spp.utils.types;
import genex;
import std;

namespace spp::parse::errors {
  SPP_EXP_CLS
  template <typename T>
  struct SyntacticErrorBuilder;
}

namespace spp::parse {
  SPP_EXP_CLS class ParserSpp;
}

namespace spp::parse::errors {
  /// Convert a token into a descriptor based on the type of
  /// token. Used in error messages about parsing. Must be TU
  /// local (not hidden) for below usage - might be GCC bug.
  SPP_ATTR_COLD inline auto TokenTypeForMessage(
    const lex::SppTokenType token)
    -> Str {
    switch (token) {
      // Special case - compiler injected so needed, but not
      // user-writable so don't offer it as an alternative.
      case lex::SppTokenType::TK_DOLLAR: return {};

      case lex::SppTokenType::LX_IDENTIFIER: return "an identifier";
      case lex::SppTokenType::LX_NUMBER: return "a number";
      case lex::SppTokenType::LX_STRING: return "a string";
      case lex::SppTokenType::LX_CHAR: return "a character";
      case lex::SppTokenType::LX_CHARACTER: return "a character";
      case lex::SppTokenType::LX_DIGIT: return "a digit";
      default: {
        auto const spelling = lex::tok_to_string(token);
        return spelling.empty() ? Str() : "'" + spelling + "'";
      }
    }
  }
}

/// The synactic error builder receives the error string from
/// the parser, and injects the expected token set into the
/// error message.
SPP_EXP_CLS template <typename T>
struct spp::parse::errors::SyntacticErrorBuilder final : utils::errors::AbstractErrorBuilder<T> {
  std::size_t Pos = 0;

  Set<lex::SppTokenType> Tokens = {};

  SPP_ATTR_COLD SyntacticErrorBuilder() = default;
  ~SyntacticErrorBuilder() override = default;

  /// Raise the syntactic-level error, doing some stringification
  /// based on tokens and their "categories", and injecting
  /// the tokens into the message.
  SPP_ATTR_COLD SPP_ATTR_NORETURN auto Raise() -> void override {
    using namespace std::string_literals;

    // Everything the parser could have accepted here, which it
    // collected as it failed each alternative in turn. Sorted,
    // and not in the order they arrive: the set is unordered,
    // so the same failure would otherwise name them differently
    // from one run to the next.
    auto token_names = Vec<Str>();
    for (auto const &token : Tokens) {
      auto name = TokenTypeForMessage(token);
      if (name.empty()) { continue; }
      if (genex::contains(token_names, name)) { continue; }
      token_names.EmplaceBack(std::move(name));
    }
    token_names |= genex::actions::sort;

    // Build the token set string by appending the token
    // (stringified) into a running string, with a ", "
    // separator (except for the end).
    auto token_set_str = Str();
    for (auto const &name : token_names) { token_set_str += (token_set_str.empty() ? "" : ", ") + name; }
    if (token_set_str.empty()) { token_set_str = "something else"; }

    // Replace the "£" with the string tokens, completing
    // the error message. Todo: Make "£" a constant.
    constexpr auto placeholder = StrView("£");
    auto err_msg = this->_ErrObj->header;
    err_msg.replace(err_msg.find(placeholder), placeholder.size(), std::move(token_set_str));

    // Inject the error message into the error object for
    // this builder, and call the internal raise steps for
    // the abstract builder.
    this->_ErrObj->messages = {
      this->_ErrFormatters[0]->ErrorRawPos(
        Pos, 1, std::move(err_msg), "Syntax error")
    };
    utils::errors::AbstractErrorBuilder<T>::Raise();
  }
};
