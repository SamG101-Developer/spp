module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.analyse.errors.semantic_error_builder;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.utils.error_formatter;
import spp.utils.errors;
import spp.utils.types;
import colex;
import genex;
import std;

use(spp::asts, struct Ast);
use(spp::analyse::scopes, class Scope);

namespace spp::analyse::errors {
  MSVC_DEVCOM_11096133_CONSTRAINT_LEXICAL_EQ
  SPP_EXP_CLS template <typename T> requires std::derived_from<T, spp::analyse::errors::SemanticError>
  struct SemanticErrorBuilder;
}

namespace spp {
  /// Build the arguments tuple from the parameter pack, which
  /// will be passed into the semantic error builder.
  SPP_EXP_FUN template <typename... Args>
  auto MakeErrArgs(Args &&... args) -> Tup<Args...> {
    return {std::forward<Args>(args)...};
  }

  /// Raise an error with a list of scopes, a deferred argument
  /// binder, and any sub-errors.
  SPP_EXP_FUN template <typename E, typename A> requires std::derived_from<E, analyse::errors::SemanticError>
  SPP_ATTR_COLD SPP_ATTR_NORETURN
  auto Raise(Vec<Scope const*> const &scopes, A &&arg_binder, Vec<Str> sub_errors = {}) -> void {
    std::apply(
      [&]<typename... Args2>(Args2 &&... unpacked_args) {
        analyse::errors::SemanticErrorBuilder<E>()
          .WithSubErrors(std::move(sub_errors))
          .WithArgs(std::forward<Args2>(unpacked_args)...).raises_from_vec(scopes);
      },
      std::forward<A>(arg_binder)());
    std::unreachable();
  }

  /// Raise an error if a condition is met. This is just a tidier
  /// version of "if (...) { Raise ... }", but it does eagerly
  /// evaluate the scopes which is sometimes an issue for nullptr
  /// usage, if the condition checks for a non-nullptr pointer
  /// that the scope belongs to.
  SPP_EXP_FUN template <typename E, typename A> requires std::derived_from<E, analyse::errors::SemanticError>
  auto RaiseIf(const bool condition, Vec<Scope const*> const &scopes, A &&arg_binder) -> void {
    if (condition) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
  }

  /// The opposite to the "RaiseIf" - this only raises an error
  /// if the condition is false.
  SPP_EXP_FUN template <typename E, typename A> requires std::derived_from<E, analyse::errors::SemanticError>
  auto RaiseUnless(const bool condition, Vec<Scope const*> const &scopes, A &&arg_binder) -> void {
    if (not condition) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
  }
}

SPP_EXP_CLS template <typename T> requires std::derived_from<T, spp::analyse::errors::SemanticError>
struct spp::analyse::errors::SemanticErrorBuilder final :
  utils::errors::AbstractErrorBuilder<T> {
  SPP_ATTR_COLD SemanticErrorBuilder() = default;

  ~SemanticErrorBuilder() override = default;

  /// Store the sub errors into an internal vector and return
  /// this object, allowing for easy chaining.
  auto WithSubErrors(Vec<Str> &&sub_errors) -> SemanticErrorBuilder& {
    _SubErrors = std::move(sub_errors);
    return *this;
  }

  /// The internal master "Raise" method for errors. This cycles
  /// formatters to meet the number of asts present, and trims
  /// the formatter list. Then, it runs the error information
  /// through the standard stringification process, before using
  /// the abstract raise function.
  SPP_ATTR_COLD SPP_ATTR_NORETURN auto Raise() -> void override {
    const auto cast_error = dynamic_cast<SemanticError*>(this->_ErrObj.get());

    // Pair the scopes' formatters, in order, with the blocks that
    // quote source (an error and its context), cycling when there
    // are fewer scopes than blocks. The header and footer quote
    // nothing, so they take no formatter; counting them used to
    // swap the two scopes of every two-scope error.
    auto messages = Vec<Str>();
    auto next = 0uz;
    for (auto const &info : cast_error->ErrorInfo) {
      if (this->_ErrFormatters.IsEmpty()) { break; }
      auto *const formatter = this->_ErrFormatters[next % this->_ErrFormatters.Len()];
      if (info.Kind == ErrorInformationKind::ERROR or info.Kind == ErrorInformationKind::CONTEXT) { ++next; }
      messages.EmplaceBack(_StringifyErrorInformation(formatter, info));
    }
    cast_error->messages = std::move(messages);

    // Format and append each per-overload sub-error consecutively
    // beneath the main error.
    auto i = 1;
    for (auto const &msg : _SubErrors) {
      auto header = std::string(50, '-') + colex::st_underline + std::string("\n\nCandidate ") + std::to_string(i)
        + ":\n" + colex::reset;
      cast_error->messages.EmplaceBack(header + msg);
      ++i;
    }

    // Throw the error object.
    utils::errors::AbstractErrorBuilder<T>::Raise();
  }

private:
  /// List of sub-errors. Todo: are these even used anymore?
  Vec<Str> _SubErrors;

  static auto _StringifyErrorInformation(
    spp::utils::errors::ErrorFormatter *formatter,
    ErrorInformation const &info)
    -> Str {
    using namespace std::string_literals;

    switch (auto [ast, kind, tag, msg] = info; kind) {
      case ErrorInformationKind::ERROR: {
        return formatter->ErrorAst(ast, std::move(msg), std::move(tag));
      }
      case ErrorInformationKind::CONTEXT: {
        return formatter->ErrorAstMinimal(ast, std::move(tag));
      }
      case ErrorInformationKind::HEADER: {
        return (colex::fg_bright_white & colex::st_bold) + std::move(msg) + ": "s + std::move(tag) + "\n"s;
      }
      case ErrorInformationKind::FOOTER: {
        return (colex::fg_bright_cyan & colex::st_bold) + "= Note: " + std::move(tag) + "\n"s +
          (colex::fg_bright_red & colex::st_bold) + "= Help: " + std::move(msg) + "\n"s;
      }
      case ErrorInformationKind::WRAPPED: {
        return std::move(tag);
      }
      default:
        std::unreachable();
    }
    return "";
  }
};
