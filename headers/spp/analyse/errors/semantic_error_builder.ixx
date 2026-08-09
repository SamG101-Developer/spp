module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.errors.semantic_error;
import spp.utils.errors;
import spp.utils.error_formatter;
import spp.utils.types;
import colex;
import genex;
import std;

namespace spp::analyse::errors {
  SPP_EXP_CLS template <typename T> requires std::derived_from<T, SemanticError>
  struct SemanticErrorBuilder;
}

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
}

namespace spp {
  SPP_EXP_FUN template <typename... Args>
  auto MakeErrArgs(Args &&... args) -> Tup<Args...> {
    return {std::forward<Args>(args)...};
  }

  SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError>
  SPP_ATTR_COLD SPP_ATTR_NORETURN auto Raise(Vec<analyse::scopes::Scope const*> const &scopes, A &&arg_binder,
    Vec<Str> sub_errors = {}) -> void {
    std::apply(
      [&]<typename... Args2>(Args2 &&... unpacked_args) {
        analyse::errors::SemanticErrorBuilder<E>()
          .WithSubErrors(std::move(sub_errors))
          .WithArgs(std::forward<Args2>(unpacked_args)...).raises_from_vec(scopes);
      },
      std::forward<A>(arg_binder)());
    std::unreachable();
  }

  SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError>
  auto RaiseIf(const bool condition, Vec<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
    if (condition) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
  }

  SPP_EXP_FUN template <typename E, typename A, typename F, typename V>
    requires std::derived_from<E, analyse::errors::SemanticError>
  auto RaiseIfAny(F &&condition, V const &vector, Vec<analyse::scopes::Scope const*> const &scopes,
    A &&arg_binder) -> void {
    for (auto const &v : vector) {
      if (condition(v)) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
    }
  }

  SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError>
  auto RaiseUnless(const bool condition, Vec<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
    if (not condition) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
  }
}

SPP_EXP_CLS template <typename T> requires std::derived_from<T, spp::analyse::errors::SemanticError>
struct spp::analyse::errors::SemanticErrorBuilder final : spp::utils::errors::AbstractErrorBuilder<T> {
  SPP_ATTR_COLD SemanticErrorBuilder() = default;

  ~SemanticErrorBuilder() override = default;

  auto WithSubErrors(Vec<Str> &&sub_errors) -> SemanticErrorBuilder& {
    _SubErrors = std::move(sub_errors);
    return *this;
  }

  SPP_ATTR_COLD SPP_ATTR_NORETURN auto Raise() -> void override {
    const auto cast_error = dynamic_cast<SemanticError*>(this->_ErrObj.get());

    // Cycle the formatters to match the number of strings being formatted.
    auto formatters = this->_ErrFormatters
      | genex::views::cycle
      | genex::views::take(cast_error->ErrorInfo.Len())
      | genex::to<Vec>();

    // Format all the error strings by the correct formatter (file agnostic).
    cast_error->messages = genex::views::zip(cast_error->ErrorInfo, std::move(formatters))
      | genex::to<Vec>()
      | genex::views::transform([](auto &&x) { return _StringifyErrorInformation(std::get<1>(x), std::get<0>(x)); })
      | genex::to<Vec>();

    // Format and append each per-overload sub-error consecutively beneath the main error.
    auto i = 1;
    for (auto const &msg : _SubErrors) {
      auto header = std::string(50, '-') + colex::st_underline + std::string("\n\nCandidate ") + std::to_string(i)
        + ":\n" + colex::reset;
      cast_error->messages.EmplaceBack(header + msg);
      ++i;
    }

    // Throw the error object.
    spp::utils::errors::AbstractErrorBuilder<T>::Raise();
  }

private:
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
  }
};
