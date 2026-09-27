module;
#include <spp/macros.hpp>

module spp.lsp.diagnostic;
import spp.analyse.errors.diagnostic_sink;
import spp.analyse.errors.semantic_error;
import spp.analyse.utils.resolution_index;
import spp.compiler.compiler;
import spp.parse.errors.parser_error;
import spp.utils.error_formatter;
import genex;
import std;

namespace spp::lsp {
  namespace {
    /// Strip off the bytes that provide ANSI colour codes
    /// on the error string. Todo: Move this to ColEx.
    auto StripAnsi(StrView text) -> Str {
      auto out = Str();
      out.reserve(text.length());
      for (auto i = 0uz; i < text.length(); ++i) {
        if (text[i] != '\x1b') {
          out += text[i];
          continue;
        }
        while (i < text.length() and text[i] != 'm') { ++i; }
      }
      return out;
    }

    /// Convert a string to be JSON compatible, the key part being
    /// the escape characters.
    auto JsonString(StrView text) -> Str {
      auto out = Str("\"");
      for (auto const c : text) {
        switch (c) {
          case '"': out += "\\\"";
            break;
          case '\\': out += "\\\\";
            break;
          case '\n': out += "\\n";
            break;
          case '\r': out += "\\r";
            break;
          case '\t': out += "\\t";
            break;
          default: {
            if (static_cast<unsigned char>(c) < 0x20u) {
              out += std::format("\\u{:04x}", static_cast<unsigned int>(static_cast<unsigned char>(c)));
            }
            else { out += c; }
          }
        }
      }
      return out + "\"";
    }

    /// Create a JSON block representing the source position
    /// information provided off of a span, with a note for
    /// compiler-generated code (preventing a 0-pos jump).
    auto JsonSpan(utils::errors::SourceSpan const &span) -> Str {
      auto out = Str("\"file\":") + JsonString(span.File);
      if (span.Generated) { return out + ",\"generated\":true"; }
      return out + std::format(
        ",\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}",
        span.StartLine, span.StartCol, span.EndLine, span.EndCol);
    }

    /// A label as json. A span with no place in the author's
    /// source says so rather than reporting line zero, which an
    /// editor would happily jump to.
    auto JsonLabel(DiagnosticLabel const &label) -> Str {
      auto out = Str("{\"primary\":") + (label.Primary ? "true" : "false");
      out += ",\"message\":" + JsonString(label.Message);
      return out + "," + JsonSpan(label.Span) + "}";
    }

    /// The members of a list, as json, shared by everything that
    /// reports a set of names.
    auto JsonMembers(
      Vec<analyse::utils::resolution_index::Member> const &members)
      -> Str {
      auto out = Str("[");
      for (auto i = 0uz; i < members.Len(); ++i) {
        auto const &member = members[i];
        out += i > 0 ? "," : "";
        out += "{\"name\":" + JsonString(member.Name);
        out += ",\"member\":" + JsonString(member.Kind);
        out += ",\"type\":" + JsonString(member.Type);
        out += ",\"definition\":{" + JsonSpan(member.Definition) + "}";

        out += ",\"signatures\":[";
        for (auto j = 0uz; j < member.Signatures.Len(); ++j) {
          out += (j > 0 ? "," : "") + JsonString(member.Signatures[j]);
        }
        out += "]}";
      }
      return out + "]";
    }
  }
}

auto spp::lsp::DiagnosticFrom(
  analyse::errors::SemanticError const &error)
  -> Diagnostic {
  using analyse::errors::ErrorInformationKind;

  auto diagnostic = Diagnostic();
  for (auto const &info : error.ErrorInfo) {
    switch (info.Kind) {
      // Add the header's message and tag into the diagnostic object.
      // Only 1 header so no data gets overridden. All ANSI stripped
      // for the raw text.
      case ErrorInformationKind::HEADER: {
        diagnostic.Code = StripAnsi(info.Msg);
        diagnostic.Title = StripAnsi(info.Tag);
        break;
      }

      // For the error part of the attached information, add a
      // diagnostic label to the list of labels on the diagnostic,
      // copying the span over.
      case ErrorInformationKind::ERROR: {
        diagnostic.Labels.EmplaceBack(
          DiagnosticLabel{.Span = info.Span, .Message = StripAnsi(info.Tag), .Primary = true});
        break;
      }

      // For each context part of the attached information, add a
      // non-primary diagnostic label to the list of labels on the
      // diagnostic, copying the span over.
      case ErrorInformationKind::CONTEXT: {
        diagnostic.Labels.EmplaceBack(
          DiagnosticLabel{.Span = info.Span, .Message = StripAnsi(info.Tag), .Primary = false});
        break;
      }

      // Add the footer information too, again simple ANSI-stripped
      // message copying.
      case ErrorInformationKind::FOOTER: {
        diagnostic.Note = StripAnsi(info.Tag);
        diagnostic.Help = StripAnsi(info.Msg);
        break;
      }

      case ErrorInformationKind::WRAPPED: {
        diagnostic.Wrapped.EmplaceBack(StripAnsi(info.Tag));
        break;
      }

      default: std::unreachable();
    }
  }
  return diagnostic;
}

auto spp::lsp::DiagnosticFrom(
  parse::errors::SyntacticError const &error)
  -> Diagnostic {
  // The syntactic error is a lot simpler (only a syntax error),
  // so manually create some text and insert the 1 diagnostic
  // label intothe diagnostic.
  auto diagnostic = Diagnostic();
  diagnostic.Code = "E0";
  diagnostic.Title = "Syntax Error";
  diagnostic.Labels.EmplaceBack(DiagnosticLabel{
    .Span = error.span,
    .Message = error.message.empty() ? StripAnsi(error.header) : StripAnsi(error.message),
    .Primary = true
  });
  return diagnostic;
}

auto spp::lsp::ToJson(Diagnostic const &diagnostic) -> Str {
  auto out = Str("{\"kind\":\"diagnostic\",\"severity\":") + JsonString(diagnostic.Severity);
  out += ",\"code\":" + JsonString(diagnostic.Code);
  out += ",\"title\":" + JsonString(diagnostic.Title);

  out += ",\"labels\":[";
  for (auto i = 0uz; i < diagnostic.Labels.Len(); ++i) {
    out += (i > 0 ? "," : "") + JsonLabel(diagnostic.Labels[i]);
  }
  out += "]";

  out += ",\"note\":" + JsonString(diagnostic.Note);
  out += ",\"help\":" + JsonString(diagnostic.Help);

  out += ",\"wrapped\":[";
  for (auto i = 0uz; i < diagnostic.Wrapped.Len(); ++i) {
    out += (i > 0 ? "," : "") + JsonString(diagnostic.Wrapped[i]);
  }
  return out + "]}";
}

auto spp::lsp::ToJson(analyse::utils::resolution_index::ResolvedName const &name) -> Str {
  auto out = Str("{\"kind\":\"symbol\",\"symbol\":") + JsonString(name.Kind);
  out += ",\"name\":" + JsonString(name.Name);
  out += ",\"type\":" + JsonString(name.Type);
  out += ",\"use\":{" + JsonSpan(name.Use) + "}";
  out += ",\"definition\":{" + JsonSpan(name.Definition) + "}";
  out += ",\"value\":" + JsonString(name.Value);
  return out + "}";
}

auto spp::lsp::ToJson(analyse::utils::resolution_index::MemberList const &members) -> Str {
  auto out = Str("{\"kind\":\"members\",\"owner\":") + JsonString(members.Owner);
  out += ",\"of\":" + JsonString(members.Of);

  return out + ",\"members\":" + JsonMembers(members.Members) + "}";
}

auto spp::lsp::ToJson(analyse::utils::resolution_index::Signature const &signature) -> Str {
  auto out = Str("{\"kind\":\"signature\",\"name\":") + JsonString(signature.Name);
  out += ",\"arguments\":{" + JsonSpan(signature.Arguments) + "}";
  return out + ",\"members\":" + JsonMembers(signature.Params) + "}";
}

auto spp::lsp::ToJson(analyse::utils::resolution_index::NamesInScope const &scope) -> Str {
  auto out = Str("{\"kind\":\"scope\",\"where\":{") + JsonSpan(scope.Where) + "}";
  return out + ",\"members\":" + JsonMembers(scope.Names) + "}";
}

auto spp::lsp::ToJson(analyse::utils::resolution_index::ComptimeValue const &value) -> Str {
  auto out = Str("{\"kind\":\"comptime\",\"name\":") + JsonString(value.Name);
  out += ",\"value\":" + JsonString(value.Value);
  return out + ",\"where\":{" + JsonSpan(value.Where) + "}}";
}

auto spp::lsp::CompileReportingJson(compiler::Compiler &c) -> bool {
  // Json catches in every build: a debugger is not what is
  // on the other end of the pipe. Enable the sink so that the
  // errors are collected rather than immediately crashing and
  // reporting.
  namespace sink = analyse::errors::diagnostic_sink;
  namespace index = analyse::utils::resolution_index;
  sink::Enable(true);

  try {
    const auto built = c.Compile();

    // Get all the errors in the sink after the compilation, and
    // convert the errors into the diagnostic objects.
    auto diagnostics = sink::Collected()
      | genex::views::transform([](auto const &error) { return DiagnosticFrom(error); })
      | genex::to<Vec>();

    const auto position_of = [](Diagnostic const &d) {
      for (auto const &label : d.Labels) {
        if (label.Primary) {
          return std::make_tuple(label.Span.File, label.Span.StartLine, label.Span.StartCol);
        }
      }
      return std::make_tuple(Str(), 0uz, 0uz);
    };
    genex::actions::sort(diagnostics, {}, position_of);

    for (auto const &diagnostic : diagnostics) { std::cout << ToJson(diagnostic) << "\n"; }
    for (auto const &name : index::GetAllEntries()) { std::cout << ToJson(name) << "\n"; }
    for (auto const &members : index::GetAllMembers()) { std::cout << ToJson(members) << "\n"; }
    for (auto const &signature : index::GetAllSignatures()) { std::cout << ToJson(signature) << "\n"; }
    for (auto const &scope : index::GetAllScopes()) { std::cout << ToJson(scope) << "\n"; }
    for (auto const &value : index::GetAllComptimeValues()) { std::cout << ToJson(value) << "\n"; }

    const auto recovered = sink::HasErrors();
    sink::Enable(false);
    return built and not recovered;
  }

  // An error from a stage with no recovery in it - the declarations
  // and the sup graph the rest is read through, or anything the
  // back end raises - still stops the compile where it was raised.
  catch (analyse::errors::SemanticError const &e) {
    for (auto const &error : sink::Collected()) {
      std::cout << ToJson(DiagnosticFrom(error)) << "\n";
    }
    std::cout << ToJson(DiagnosticFrom(e)) << "\n";
    sink::Enable(false);
    return false;
  }

  catch (parse::errors::SyntacticError const &e) {
    std::cout << ToJson(DiagnosticFrom(e)) << "\n";
    sink::Enable(false);
    return false;
  }
}
