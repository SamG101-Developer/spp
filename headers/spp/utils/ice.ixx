module;
#include <spp/macros.hpp>

export module spp.utils.ice;
import spp.utils.error_formatter;
import spp.utils.types;
import std;

use(spp, struct InternalCompilerError);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct Ast);

/// An internal compiler error: the compiler reached a state its own code says cannot happen. It is a bug in the
/// compiler, not in the program being compiled, so it is no "SemanticError": nothing that recovers from a user's error
/// (the editor's per-member recovery, an overload candidate that does not fit) can catch it, and only the top level
/// reports it. It is rendered when raised, as what it points at may be freed before anything catches it.
SPP_EXP_CLS struct spp::InternalCompilerError final : std::exception {
  /// What the raising code says went wrong.
  Str Message;

  /// Where in the compiler it was raised.
  std::source_location RaisedAt;

  /// The place in the program being compiled the compiler was looking at, when it was given one with a source
  /// position (an editor underlines it).
  Opt<utils::errors::SourceSpan> Span;

  /// The whole report: the message, the quoted source, where it was raised, and the call stack.
  Str Text;

  SPP_ATTR_NODISCARD auto what() const noexcept -> char const* override {
    return Text.c_str();
  }
};

namespace spp {
  /// Stop with an internal compiler error. Called where the compiler's own state is impossible, never for a mistake
  /// in the source being compiled (that is a "SemanticError"). The report names the compiler file, line and function
  /// that raised it, and the call stack where the standard library can give one.
  SPP_EXP_FUN SPP_ATTR_COLD SPP_ATTR_NORETURN auto Ice(
    StrView message, std::source_location raised_at = std::source_location::current()) -> void;

  /// An internal compiler error at "at" in the program: quoted from its own scope's source when it has one ("Ast::
  /// GetAstScope"), else written out ("ToString") with no position.
  SPP_EXP_FUN SPP_ATTR_COLD SPP_ATTR_NORETURN auto Ice(
    asts::Ast const &at, StrView message, std::source_location raised_at = std::source_location::current()) -> void;

  /// An internal compiler error at "at", quoted from the source of "scope"'s module: for an ast with no scope of its
  /// own (most expressions), read where the scope that holds it is.
  SPP_EXP_FUN SPP_ATTR_COLD SPP_ATTR_NORETURN auto Ice(
    asts::Ast const &at, analyse::scopes::Scope const &scope, StrView message,
    std::source_location raised_at = std::source_location::current()) -> void;
}
