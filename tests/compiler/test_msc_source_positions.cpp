#include "test_macros.hpp"

import spp.asts.identifier_ast;
import spp.lex.lexer;
import spp.utils.error_formatter;

// Where a position is in its file: the line and column debug info and errors give it. A line is counted as the
// author's editor counts it, so a newline inside a string literal (a character token, not a line feed) ends one too.

namespace {
  using spp::utils::errors::ErrorFormatter;

  auto FormatterFor(spp::Str const &code, spp::StrView prelude = "") -> spp::Unique<ErrorFormatter> {
    // Lexed first: the prelude's index is only known once it has been.
    auto lexer = spp::lex::Lexer(code, prelude);
    auto tokens = lexer.Lex();
    return ErrorFormatter::ForFile(std::move(tokens), "positions.spp", lexer.PreludeTokenIndex());
  }

  /// The token index of the first token reading "data" (one character, as an identifier's letters are lexed), the
  /// "nth" one counting from zero.
  auto TokenIndexOf(spp::Str const &code, spp::StrView data, std::size_t nth = 0) -> std::size_t {
    const auto tokens = spp::lex::Lexer(code).Lex();
    for (auto i = 0uz; i < tokens.size(); ++i) {
      if (tokens[i].Data == data and nth-- == 0) { return i; }
    }
    ADD_FAILURE() << "no token reads \"" << data << "\"";
    return 0;
  }
}

TEST(TestSourcePositions, test_line_and_column_of_a_token) {
  const auto code = spp::Str("let x = 1\n  let y = 2\n");
  const auto file = FormatterFor(code);
  const auto at = file->LineColOf(file->PositionOf(TokenIndexOf(code, "y")));
  ASSERT_TRUE(at.has_value());
  EXPECT_EQ(at->Line, 2uz);
  EXPECT_EQ(at->Column, 7uz);
}

TEST(TestSourcePositions, test_a_line_after_a_multi_line_string) {
  const auto code = spp::Str("let s = \"a\nb\"\nlet z = 1\n");
  const auto file = FormatterFor(code);
  const auto index = TokenIndexOf(code, "z");
  const auto at = file->LineColOf(file->PositionOf(index));
  ASSERT_TRUE(at.has_value());
  EXPECT_EQ(at->Line, 3uz);
  EXPECT_EQ(at->Column, 5uz);

  // An error quotes the same line, the string's second line no part of it.
  const auto z = spp::asts::IdentifierAst(file->PositionOf(index), "z");
  const auto quoted = file->ErrorAst(&z, "", "here");
  EXPECT_NE(quoted.find("on line 3"), spp::Str::npos) << quoted;
  EXPECT_NE(quoted.find("let z = 1"), spp::Str::npos) << quoted;
  EXPECT_EQ(quoted.find("b\""), spp::Str::npos) << quoted;
}

TEST(TestSourcePositions, test_a_position_inside_a_multi_line_string) {
  const auto code = spp::Str("let s = \"a\nbc\"\n");
  const auto file = FormatterFor(code);
  const auto at = file->LineColOf(file->PositionOf(TokenIndexOf(code, "c")));
  ASSERT_TRUE(at.has_value());
  EXPECT_EQ(at->Line, 2uz);
  EXPECT_EQ(at->Column, 2uz);
}

TEST(TestSourcePositions, test_the_whole_file_ends_on_its_last_line) {
  // Zero-based, as an editor counts: after a trailing newline, the empty line it starts is the last.
  EXPECT_EQ(FormatterFor("let a = 1\nlet b = 2\n")->SpanOfWholeFile().EndLine, 2uz);
  EXPECT_EQ(FormatterFor("let a = 1\nlet b = 2")->SpanOfWholeFile().EndLine, 1uz);
  EXPECT_EQ(FormatterFor("let a = 1\nlet b = 2", "use std::x")->SpanOfWholeFile().EndLine, 1uz);
}

TEST(TestSourcePositions, test_no_line_outside_the_authors_source) {
  const auto code = spp::Str("let a = 1\n");
  const auto file = FormatterFor(code, "use std::number::S32");

  // The lexer's leading newline, the prelude appended behind the author's code, and a position of no file.
  auto lexer = spp::lex::Lexer(code, "use std::number::S32");
  static_cast<void>(lexer.Lex());
  EXPECT_FALSE(file->LineColOf(file->PositionOf(0)).has_value());
  EXPECT_FALSE(file->LineColOf(file->PositionOf(lexer.PreludeTokenIndex() + 1)).has_value());
  EXPECT_FALSE(file->LineColOf(3uz).has_value());
  EXPECT_TRUE(file->LineColOf(file->PositionOf(1)).has_value());
}

TEST(TestSourcePositions, test_double_hash_is_a_line_comment) {
  // "##" was a block comment, whose newlines the lexer dropped; it is now a line comment like any "#".
  const auto code = spp::Str("## not code\nlet y = 1\n");
  const auto file = FormatterFor(code);
  const auto at = file->LineColOf(file->PositionOf(TokenIndexOf(code, "y")));
  ASSERT_TRUE(at.has_value());
  EXPECT_EQ(at->Line, 2uz);
  for (auto const &token : spp::lex::Lexer(code).Lex()) { EXPECT_NE(token.Data, "o") << "the comment was lexed"; }
}
