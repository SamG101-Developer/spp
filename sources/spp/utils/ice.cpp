module;
#include <spp/macros.hpp>

module spp.utils.ice;
import spp.analyse.scopes.scope;
import spp.asts.ast;
import spp.utils.error_formatter;
import colex;
import std;

namespace spp {
  namespace {
    /// The raising file from the repository's "sources" or "headers" folder on ("sources/spp/cli.cpp"): the absolute
    /// path of the machine that built the compiler means nothing to whoever reads the report.
    auto CompilerFileOf(
      std::source_location const &raised_at) -> Str {
      // MSVC names the file with "\\" separators.
      auto path = Str(raised_at.file_name());
      genex::actions::replace(path, '\\', '/');
      for (const auto root : {StrView("/sources/spp/"), StrView("/headers/spp/")}) {
        if (const auto at = path.find(root); at != Str::npos) { return path.substr(at + 1); }
      }
      return path;
    }

    /// The call stack of the caller of "Ice", its own frame skipped, or a line saying this build has none to give.
    /// "std::stacktrace" is used where the standard library is known to provide it (libstdc++, linked with
    /// "stdc++exp"). A macro, not a function: it is taken in the frame of "Ice" itself, so no helper's frame (inlined
    /// or not) is counted in what is skipped.
#if SPP_COMPILER_GCC
#define SPP_ICE_CALL_STACK() std::to_string(std::stacktrace::current(1))
#else
#define SPP_ICE_CALL_STACK() \
  Str("  stack trace unavailable: this build's standard library does not provide std::stacktrace")
#endif

    /// Render and throw the error, given the source quote (empty for none) and span the overload found.
    SPP_ATTR_NORETURN auto Throw(
      const StrView message, std::source_location const &raised_at, Opt<utils::errors::SourceSpan> span,
      Str const &quoted, Str const &call_stack) -> void {
      auto ice = InternalCompilerError();
      ice.Message = Str(message);
      ice.RaisedAt = raised_at;
      ice.Span = std::move(span);
      ice.Text = (colex::fg_bright_white & colex::st_bold) + Str("Internal Compiler Error: ") + Str(message) + "\n"
        + quoted
        + (colex::fg_bright_cyan & colex::st_bold) + Str("= Raised at: ") + CompilerFileOf(raised_at) + ":"
        + std::to_string(raised_at.line()) + " in " + raised_at.function_name() + "\n"
        + Str("= Call stack:\n") + call_stack + "\n"
        + (colex::fg_bright_red & colex::st_bold)
        + Str("= Help: This is a bug in the compiler, not in your code. Please report it with the code that caused it.")
        + "\x1b[0m";
      throw ice;
    }

    /// Quote "at" through "scope"'s module, or write it out when there is no scope to read it in.
    SPP_ATTR_NORETURN auto ThrowAt(
      asts::Ast const &at, analyse::scopes::Scope const *scope, const StrView message,
      std::source_location const &raised_at, Str const &call_stack) -> void {
      // The file the ast was parsed from, else the scope's.
      auto formatter = utils::errors::ErrorFormatter::Owning(&at);
      if (formatter == nullptr and scope != nullptr) { formatter = scope->GetErrorFormatter(); }
      if (formatter == nullptr) {
        Throw(message, raised_at, Nullopt, "= At (no source position): " + at.ToString() + "\n", call_stack);
      }
      const auto quoted = formatter->ErrorAst(&at, "", "The compiler was looking at this");
      Throw(message, raised_at, formatter->SpanOfAst(&at), quoted + "\n", call_stack);
    }
  }
}

auto spp::Ice(
  const StrView message, const std::source_location raised_at) -> void {
  Throw(message, raised_at, Nullopt, "", SPP_ICE_CALL_STACK());
}

auto spp::Ice(
  asts::Ast const &at, const StrView message, const std::source_location raised_at) -> void {
  ThrowAt(at, at.GetAstScope(), message, raised_at, SPP_ICE_CALL_STACK());
}

auto spp::Ice(
  asts::Ast const &at, analyse::scopes::Scope const &scope, const StrView message,
  const std::source_location raised_at) -> void {
  ThrowAt(at, &scope, message, raised_at, SPP_ICE_CALL_STACK());
}
