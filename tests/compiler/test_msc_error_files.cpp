#include "test_macros.hpp"

import spp.asts.identifier_ast;
import spp.lex.lexer;
import spp.utils.error_formatter;

// Each source file's positions are its own: an ast names the file it was parsed from, so an error quotes it from that
// file whichever scopes it was raised with, and another file's formatter has no line of its to show.

namespace {
  using spp::utils::errors::ErrorFormatter;

  auto FormatterFor(spp::Str const &code, spp::Str const &path) -> spp::Unique<ErrorFormatter> {
    // Lexed first: the prelude's index is only known once it has been.
    auto lexer = spp::lex::Lexer(code);
    auto tokens = lexer.Lex();
    return ErrorFormatter::ForFile(std::move(tokens), path, lexer.PreludeTokenIndex());
  }
}

TEST(TestErrorFiles, test_an_ast_is_quoted_from_its_own_file) {
  const auto first = FormatterFor("let in_the_first_file = 1\n", "first.spp");
  const auto second = FormatterFor("let in_the_second_file = 2\n", "second.spp");
  const auto third = FormatterFor("let in_the_third_file = 3\n", "third.spp");

  // An ast on the third file's first line.
  const auto in_third = spp::asts::IdentifierAst(third->PositionOf(1), "let");
  EXPECT_EQ(ErrorFormatter::Owning(&in_third), third.get());

  const auto quoted = third->ErrorAst(&in_third, "", "here");
  EXPECT_NE(quoted.find("third.spp"), spp::Str::npos) << quoted;
  EXPECT_NE(quoted.find("in_the_third_file"), spp::Str::npos) << quoted;

  // Through another file's formatter it has no position, rather than that file's text.
  const auto elsewhere = second->ErrorAst(&in_third, "", "here");
  EXPECT_EQ(elsewhere.find("in_the_second_file"), spp::Str::npos) << elsewhere;
  EXPECT_TRUE(second->SpanOfAst(&in_third).Generated);
  EXPECT_FALSE(third->SpanOfAst(&in_third).Generated);
  EXPECT_EQ(ErrorFormatter::Owning(first->PositionOf(1)), first.get());
}

TEST(TestErrorFiles, test_positions_no_file_holds_are_generated) {
  // Generated code (0) and code the compiler parses for itself (small positions, from a formatter owning none).
  const auto file = FormatterFor("let x = 1\n", "file.spp");
  EXPECT_EQ(ErrorFormatter::Owning(0uz), nullptr);
  EXPECT_EQ(ErrorFormatter::Owning(3uz), nullptr);
  EXPECT_EQ(ErrorFormatter::Owning(file->PositionOf(1)), file.get());
}

TEST(TestErrorFiles, test_a_freed_file_owns_nothing) {
  auto position = 0uz;
  {
    const auto gone = FormatterFor("let x = 1\n", "gone.spp");
    position = gone->PositionOf(1);
    EXPECT_EQ(ErrorFormatter::Owning(position), gone.get());
  }
  EXPECT_EQ(ErrorFormatter::Owning(position), nullptr);
}

TEST(TestErrorFiles, test_a_prelude_line_is_quoted_as_the_preludes) {
  // The prelude is lexed behind the file's own code, so its positions are the file's; an error in one of its lines
  // (open-bugs #65: a prelude name this std does not define) quotes that line, as the prelude's, not the end of the
  // file with nothing quoted.
  auto lexer = spp::lex::Lexer("fun main() -> Void { }\n", "\nuse std::number::F128\n");
  auto tokens = lexer.Lex();
  auto at = 0uz;
  // The raw lexer makes a token of each letter: "F128" starts at its "F".
  while (at < tokens.size() and tokens[at].Data != "F") { ++at; }
  ASSERT_LT(at, tokens.size());
  ASSERT_GE(at, lexer.PreludeTokenIndex());
  const auto file = ErrorFormatter::ForFile(std::move(tokens), "main.spp", lexer.PreludeTokenIndex());

  const auto name = spp::asts::IdentifierAst(file->PositionOf(at), "F128");
  const auto quoted = spp_test::StripAnsi(file->ErrorAst(&name, "", "here"));
  EXPECT_NE(quoted.find("in the prelude"), spp::Str::npos) << quoted;
  EXPECT_NE(quoted.find("use std::number::F128"), spp::Str::npos) << quoted;
  EXPECT_NE(quoted.find("^^^^ <- here"), spp::Str::npos) << quoted;
  EXPECT_EQ(quoted.find("end of the file"), spp::Str::npos) << quoted;

  // Still no line of the file the author wrote, for an editor.
  EXPECT_TRUE(file->SpanOfAst(&name).Generated);
  EXPECT_TRUE(file->IsPastUserSource(name.PosStart()));
}
