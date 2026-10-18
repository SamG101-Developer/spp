#include "test_macros.hpp"

import spp.asts.identifier_ast;
import spp.lsp.diagnostic;
import spp.utils.errors;
import spp.utils.ice;

// An internal compiler error is a compiler bug, not a mistake in the program: it is no "SemanticError", so what
// recovers from one (the editor's per-member recovery, an overload candidate that does not fit) cannot catch it, and
// its report says where in the compiler it was raised.

namespace {
  /// The error "body" raises, failing the test if it raises nothing or something else.
  template <typename F>
  auto IceFrom(F &&body) -> spp::InternalCompilerError {
    try { body(); }
    catch (spp::InternalCompilerError const &e) { return e; }
    ADD_FAILURE() << "Expected an internal compiler error";
    return {};
  }
}

TEST(TestIce, test_not_caught_as_a_user_error) {
  auto caught_as_user_error = false;
  try {
    try { spp::Ice("impossible state"); }
    catch (spp::analyse::errors::SemanticError const &) { caught_as_user_error = true; }
    catch (spp::utils::errors::AbstractError const &) { caught_as_user_error = true; }
  }
  catch (spp::InternalCompilerError const &) {}
  EXPECT_FALSE(caught_as_user_error);
}

TEST(TestIce, test_report_names_where_it_was_raised) {
  const auto line = std::source_location::current().line() + 1;
  const auto ice = IceFrom([] { spp::Ice("impossible state"); });
  EXPECT_EQ(ice.Message, "impossible state");
  EXPECT_EQ(ice.RaisedAt.line(), line);
  const auto text = spp::Str(ice.what());
  EXPECT_NE(text.find("Internal Compiler Error: impossible state"), spp::Str::npos);
  EXPECT_NE(text.find("tests/compiler/test_msc_ice.cpp:" + std::to_string(line)), spp::Str::npos) << text;
  EXPECT_NE(text.find("= Call stack:"), spp::Str::npos);
#if SPP_COMPILER_GCC
  EXPECT_EQ(text.find("stack trace unavailable"), spp::Str::npos) << text;
#endif
}

TEST(TestIce, test_assert_fires_in_every_build) {
  const auto ice = IceFrom([] {
    const auto holds = false;
    SPP_ASSERT(holds);
  });
  EXPECT_EQ(ice.Message, "Assertion failed: holds");
}

TEST(TestIce, test_ast_with_no_scope_is_written_out) {
  const auto at = spp::asts::IdentifierAst(0, "seen_here");
  const auto ice = IceFrom([&] { spp::Ice(at, "impossible state"); });
  EXPECT_FALSE(ice.Span.has_value());
  EXPECT_NE(spp::Str(ice.what()).find("(no source position): seen_here"), spp::Str::npos) << ice.what();
}

TEST(TestIce, test_editor_diagnostic) {
  const auto ice = IceFrom([] { spp::Ice("impossible state"); });
  const auto diagnostic = spp::lsp::DiagnosticFrom(ice);
  EXPECT_EQ(diagnostic.Code, "ICE");
  EXPECT_EQ(diagnostic.Title, "Internal Compiler Error");
  EXPECT_NE(diagnostic.Note.find("impossible state (raised at "), spp::Str::npos) << diagnostic.Note;
}
