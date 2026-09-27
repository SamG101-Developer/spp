module;
#include <spp/macros.hpp>

export module spp.lsp.diagnostic;
import spp.analyse.errors.semantic_error;
import spp.analyse.utils.resolution_index;
import spp.compiler.compiler;
import spp.parse.errors.parser_error;
import spp.utils.error_formatter;
import spp.utils.types;
import std;

use(spp::lsp, struct DiagnosticLabel);
use(spp::lsp, struct Diagnostic);

/// One labelled place in an error: where it is reported, or
/// somewhere that explains it ("first defined here"). The
/// primary label is what an editor underlines; the rest are
/// what the protocol calls related information, and what the
/// terminal renders as the context blocks above the error.
SPP_EXP_CLS struct spp::lsp::DiagnosticLabel {
  utils::errors::SourceSpan Span;
  Str Message;
  bool Primary = false;
};

/// An error as data rather than as a rendered block: the same
/// information the terminal prints, in the shape a diagnostic
/// takes. One consumer is "--message-format=json"; the other is
/// the language server, which publishes these directly.
SPP_EXP_CLS struct spp::lsp::Diagnostic {
  Str Code;
  Str Title;
  Str Severity = "error";
  Vec<DiagnosticLabel> Labels;
  Str Note;
  Str Help;
  Vec<Str> Wrapped;
};

namespace spp::lsp {
  /// Read a raised semantic error as a diagnostic. Everything
  /// comes off the error's own information list, which carries
  /// the spans from the moment it was raised.
  SPP_EXP_FUN auto DiagnosticFrom(analyse::errors::SemanticError const &error) -> Diagnostic;

  /// Read a raised syntax error as a diagnostic. The parser
  /// reports one place and one expected-token message, so
  /// this has a single label and no context.
  SPP_EXP_FUN auto DiagnosticFrom(parse::errors::SyntacticError const &error) -> Diagnostic;

  /// Render a diagnostic as one line of json.
  SPP_EXP_FUN auto ToJson(Diagnostic const &diagnostic) -> Str;

  /// Render what one name resolved to as one line of json.
  /// Shares the output with the diagnostics, so both carry
  /// a "kind" telling a reader which of the two it has.
  SPP_EXP_FUN auto ToJson(analyse::utils::resolution_index::ResolvedName const &name) -> Str;

  /// Render what one type or namespace holds as one line of
  /// json - the list an editor offers after a "." or a "::".
  SPP_EXP_FUN auto ToJson(analyse::utils::resolution_index::MemberList const &members) -> Str;

  /// Render what a call resolved to as one line of json:
  /// where its arguments are, and the parameters they fill.
  SPP_EXP_FUN auto ToJson(analyse::utils::resolution_index::Signature const &signature) -> Str;

  /// Render what a "cmp" declaration computed as one line
  /// of json.
  SPP_EXP_FUN auto ToJson(analyse::utils::resolution_index::ComptimeValue const &value) -> Str;

  /// Render what one part of a file can name as one line of
  /// json.
  SPP_EXP_FUN auto ToJson(analyse::utils::resolution_index::NamesInScope const &scope) -> Str;

  /// Run a compilation for something reading the output rather
  /// than someone looking at it: every diagnostic, then the
  /// resolution index, each as one line of json on stdout.
  SPP_EXP_FUN auto CompileReportingJson(compiler::Compiler &c) -> bool;
}
