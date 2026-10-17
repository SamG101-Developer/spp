module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.analyse.errors.semantic_error_builder;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.utils.error_formatter;
import spp.utils.errors;
import spp.utils.ice;
import spp.utils.types;
import colex;
import genex;
import std;

use(spp::asts, struct Ast);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeRef);

namespace spp::analyse::errors {
  MSVC_DEVCOM_11096133_CONSTRAINT_LEXICAL_EQ
  SPP_EXP_CLS template <typename T> requires std::derived_from<T, spp::analyse::errors::SemanticError>
  struct SemanticErrorBuilder;
}

namespace spp {
  /// A resolved type given to an error in place of a type ast:
  /// named as seen from "In" ("TypeRef::AstIn") and placed at
  /// "At", so an error that points at the type points there.
  /// Held by value, as "ERR_ARGS" builds its arguments before the
  /// error is. Made by "ErrTypeAt".
  SPP_EXP_CLS template <typename R = analyse::scopes::TypeRef>
  struct ErrType {
    R Ref;
    asts::Ast const *At = nullptr;
    analyse::scopes::Scope const *In = nullptr;
  };

  /// "ref" for an error message, named in "in" and placed at "at"
  /// ("ErrType").
  SPP_EXP_FUN template <typename R>
  auto ErrTypeAt(R const &ref, asts::Ast const &at, analyse::scopes::Scope const &in) -> ErrType<R> {
    return {ref, &at, &in};
  }

  /// Build the arguments tuple from the parameter pack, which
  /// will be passed into the semantic error builder.
  SPP_EXP_FUN template <typename... Args>
  auto MakeErrArgs(Args &&... args) -> Tup<Args...> {
    return {std::forward<Args>(args)...};
  }

  namespace detail {
    template <typename T>
    constexpr auto kIsErrType = false;

    template <typename R>
    constexpr auto kIsErrType<ErrType<R>> = true;
  }

  /// Raise an error from a deferred argument binder, and any
  /// sub-errors. Each ast the error quotes is quoted from the file
  /// it was parsed from, so no scope is needed to find it.
  SPP_EXP_FUN template <typename E, typename A> requires std::derived_from<E, analyse::errors::SemanticError>
  SPP_ATTR_COLD SPP_ATTR_NORETURN
  auto Raise(A &&arg_binder, Vec<Str> sub_errors = {}) -> void {
    // A resolved type ("ErrType") is named in the scope it was
    // given, and kept alive for the build.
    auto named = Vec<Shared<void const>>();
    const auto adapt = [&]<typename T>(T &&arg) -> decltype(auto) {
      if constexpr (detail::kIsErrType<std::remove_cvref_t<T>>) {
        auto type = arg.Ref.AstIn(*arg.In);
        SPP_ASSERT(type != nullptr);
        if (arg.At != nullptr) { type = type->WithSourceSpanAt(*arg.At); }
        named.emplace_back(type);
        return static_cast<asts::Ast const&>(*type);
      }
      else { return std::forward<T>(arg); }
    };
    apply(
      [&]<typename... Args2>(Args2 &&... unpacked_args) {
        analyse::errors::SemanticErrorBuilder<E>()
          .WithSubErrors(std::move(sub_errors))
          .WithArgs(adapt(std::forward<Args2>(unpacked_args))...).Raise();
      },
      std::forward<A>(arg_binder)());
    std::unreachable();
  }

  /// Raise an error if a condition is met: a tidier "if (...) {
  /// Raise ... }". The arguments are bound only if it is.
  SPP_EXP_FUN template <typename E, typename A> requires std::derived_from<E, analyse::errors::SemanticError>
  auto RaiseIf(const bool condition, A &&arg_binder) -> void {
    if (condition) { Raise<E>(std::forward<A>(arg_binder)); }
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

  /// The internal master "Raise" method for errors. It quotes
  /// each block through its ast's file, runs the error
  /// information through the standard stringification process,
  /// then uses the abstract raise function.
  SPP_ATTR_COLD SPP_ATTR_NORETURN auto Raise() -> void override {
    const auto cast_error = dynamic_cast<SemanticError*>(this->_ErrObj.get());

    // Each block that quotes source (an error and its context) is
    // quoted through the file its ast was parsed from, which its
    // position names ("ErrorFormatter::Owning"). An ast no file
    // holds (generated) is written out instead. The header and
    // footer quote nothing, so they take no formatter.
    auto messages = Vec<Str>();
    auto unowned = Vec<ErrorInformation*>();
    auto file = Str();
    for (auto &info : cast_error->ErrorInfo) {
      auto formatter = static_cast<utils::errors::ErrorFormatter*>(nullptr);
      if (info.Kind == ErrorInformationKind::ERROR or info.Kind == ErrorInformationKind::CONTEXT) {
        formatter = utils::errors::ErrorFormatter::Owning(info.Ast);
        if (formatter == nullptr) {
          messages.emplace_back(utils::errors::ErrorFormatter::WithoutSource(info.Ast, info.Tag));
          unowned.emplace_back(&info);
          continue;
        }
        info.Span = formatter->SpanOfAst(info.Ast);
        if (file.empty()) { file = info.Span.File; }
      }
      messages.emplace_back(_StringifyErrorInformation(formatter, info));
    }
    cast_error->Messages = std::move(messages);

    // A generated block's diagnostic is filed under the error's
    // first block from a file, marked as generated, so an editor
    // shows it beside the rest of the error.
    for (auto *info : unowned) { info->Span = utils::errors::SourceSpan{.File = file}; }

    // Format and append each per-overload sub-error consecutively
    // beneath the main error.
    auto i = 1;
    for (auto const &msg : _SubErrors) {
      auto header = Str(50, '-') + colex::st_underline + Str("\n\nCandidate ") + std::to_string(i)
        + ":\n" + colex::reset;
      cast_error->Messages.emplace_back(header + msg);
      ++i;
    }

    // Throw the error object.
    utils::errors::AbstractErrorBuilder<T>::Raise();
  }

private:
  /// List of sub-errors (the failed overloads of a call).
  Vec<Str> _SubErrors;

  static auto _StringifyErrorInformation(
    spp::utils::errors::ErrorFormatter *formatter,
    ErrorInformation const &info)
    -> Str {
    using namespace std::string_literals;

    // A copy, because the rendering moves the strings out of it,
    // and the error keeps its own for the consumers that read
    // the information rather than the message.
    auto parts = info;
    switch (parts.Kind) {
      case ErrorInformationKind::ERROR: {
        return formatter->ErrorAst(parts.Ast, std::move(parts.Msg), std::move(parts.Tag));
      }
      case ErrorInformationKind::CONTEXT: {
        return formatter->ErrorAstMinimal(parts.Ast, std::move(parts.Tag));
      }
      case ErrorInformationKind::HEADER: {
        return (colex::fg_bright_white & colex::st_bold) + std::move(parts.Msg) + ": "s + std::move(parts.Tag) + "\n"s;
      }
      case ErrorInformationKind::FOOTER: {
        return (colex::fg_bright_cyan & colex::st_bold) + "= Note: " + std::move(parts.Tag) + "\n"s +
          (colex::fg_bright_red & colex::st_bold) + "= Help: " + std::move(parts.Msg) + "\n"s;
      }
      case ErrorInformationKind::WRAPPED: {
        return std::move(parts.Tag);
      }
      default:
        Ice(std::format("unhandled error information kind {}", static_cast<int>(parts.Kind)));
    }
  }
};
