module;
#include <spp/macros.hpp>

export module spp.utils.errors;
import spp.utils.error_formatter;
import spp.utils.types;
import genex;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::utils::errors, struct AbstractError);
use(spp::utils::errors, template <typename T> struct AbstractErrorBuilder;);

/// The lowest level error type, the abstract error: contains
/// the key features of an error raised in the S++ compiler;
/// either a syntactic or semantic error as of right now.
SPP_EXP_CLS struct spp::utils::errors::AbstractError : std::runtime_error {
  Vec<Str> messages;
  Str final_message;

  SPP_ATTR_COLD AbstractError() : std::runtime_error("") {}

  SPP_ATTR_COLD AbstractError(AbstractError const &) = default;

  ~AbstractError() override = default;

  [[nodiscard]]

  /// Override the string ".what()" for C++ compatibility in
  /// the error system.
  auto what() const noexcept -> const char* override {
    return final_message.c_str();
  }
};

/// The abstract error builder: the base class for all error
/// builders. Lots of method to help "build" an error.
SPP_EXP_CLS template <typename T>
struct spp::utils::errors::AbstractErrorBuilder {
  SPP_ATTR_COLD AbstractErrorBuilder() = default;
  virtual ~AbstractErrorBuilder() = default;

  /// Provide the arguments into the error object when
  /// constructing it. Needed for the internal error to exist.
  template <typename... Args> // requires std::constructible_from<T, Args...>
  auto WithArgs(Args &&... args) -> AbstractErrorBuilder& {
    // Provide the arguments to construct the error object.
    _ErrObj = MakeUnique<T>(std::forward<Args>(args)...);
    return *this;
  }

  /// Inject the error formatters into the list - this allows
  /// for inter-module context, using the different module's
  /// token sets.
  auto WithFormatters(Vec<ErrorFormatter*> const &formatters) -> AbstractErrorBuilder& {
    // Bind the error formatters to the builder.
    _ErrFormatters = formatters;
    return *this;
  }

  /// Add a single error formatter to the list, rather then
  /// set the list to a new one. Todo: Rename.
  auto WithErrorFormatter(ErrorFormatter *error_formatter) -> AbstractErrorBuilder& {
    // Add a single error formatter to the list.
    _ErrFormatters.EmplaceBack(error_formatter);
    return *this;
  }

  /// Throw the error object, with the combined internal error
  /// object's messages stacked and thrown within the "T" type,
  /// "T" being the actual error type.
  SPP_ATTR_COLD SPP_ATTR_NORETURN
  virtual auto Raise() -> void {
    // Throw the error object. Terminated with an explicit reset: the
    // message is written in colour, so the reset is needed so the
    // console can go back to how it was once s++ is done.
    this->_ErrObj->final_message = (this->_ErrObj->messages
      | genex::views::join_with('\n')
      | genex::to<Str>()) + "\x1b[0m";
    throw T(*_ErrObj);
  }

protected:
  /// The error object, constructed when args are provided.
  Unique<T> _ErrObj;

  /// The list of the error formatters for each err object
  /// internal part.
  Vec<ErrorFormatter*> _ErrFormatters;
};
