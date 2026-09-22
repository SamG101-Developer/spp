#include "../test_macros.hpp"

namespace {
  /// Compile "code", expect a type mismatch, and answer its
  /// message as plain text (empty, and a failure, if none).
  auto TypeMismatchText(std::string code) -> std::string {
    try {
      build_temp_project(std::move(code));
    }
    catch (spp::analyse::errors::SppTypeMismatchError const &e) {
      return spp_test::StripAnsi(e.what());
    }
    ADD_FAILURE() << "Expected a type mismatch error.";
    return {};
  }
}

// A type written in source is shown as written, with the qualified form it resolved to beside it.

TEST(TestErrorMessages, test_written_type_shown_with_its_qualified_form) {
  const auto text = TypeMismatchText(R"(
    fun f() -> Void {
        let x: S32 = false
    }
)");
  EXPECT_NE(text.find("Expected type S32 (aka std::num::sized_integer::SizedInteger[w=32_u32, signed=true])"),
    std::string::npos) << text;
}

TEST(TestErrorMessages, test_alias_shown_by_its_own_name) {
  const auto text = TypeMismatchText(R"(
    type Num = S32
    fun f() -> Void {
        let x: Num = false
    }
)");
  EXPECT_NE(text.find("Expected type Num (aka std::num::sized_integer::SizedInteger[w=32_u32, signed=true])"),
    std::string::npos) << text;
}

// Nothing to add when the written text already is the qualified form, or when nothing was written.

TEST(TestErrorMessages, test_fully_qualified_type_has_no_aka) {
  const auto text = TypeMismatchText(R"(
    fun f() -> Void {
        let x: std::boolean::Bool = 1
    }
)");
  EXPECT_NE(text.find("Expected type std::boolean::Bool"), std::string::npos) << text;
  EXPECT_EQ(text.find("(aka"), std::string::npos) << text;
}

// "Self" is shown as written, with the class it stands for beside it.

TEST(TestErrorMessages, test_self_shown_with_its_class) {
  const auto text = TypeMismatchText(R"(
    cls A { }
    sup A {
        !public fun m(&self) -> Void {
            let x: Self = false
        }
    }
)");
  EXPECT_NE(text.find("Expected type Self (aka "), std::string::npos) << text;
}

TEST(TestErrorMessages, test_self_generic_argument_shown_as_written) {
  const auto text = TypeMismatchText(R"(
    cls Box[T] { !public v: T }
    sup [T] Box[T] {
        !public fun m(&self) -> Void {
            let x: Vec[Self] = false
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m()
    }
)");
  EXPECT_NE(text.find("Expected type Vec[Self] (aka "), std::string::npos) << text;
}

// Re-importing a name the prelude already brings in shows only the author's "use", never the prelude's.

TEST(TestErrorMessages, test_reimporting_a_prelude_name_points_at_the_authors_import) {
  auto text = std::string();
  try {
    build_temp_project(R"(
    use std::annotations::abstract_method
    fun f() -> Void { }
)");
  }
  catch (spp::analyse::errors::SppIdentifierDuplicateError const &e) {
    text = spp_test::StripAnsi(e.what());
  }
  ASSERT_FALSE(text.empty()) << "Expected an identifier duplicate error.";
  EXPECT_NE(text.find("The prelude already imports"), std::string::npos) << text;
  EXPECT_EQ(text.find("<end of file>"), std::string::npos) << text;
  EXPECT_EQ(text.find("First "), std::string::npos) << text;
}

// An error's context blocks come before its own block, and the note and help close it.

TEST(TestErrorMessages, test_move_from_borrowed_memory_shows_the_borrow_before_the_note) {
  auto text = std::string();
  try {
    build_temp_project(R"(
    cls T { }
    cls A { !public v: T }
    fun f(a: &A) -> Void {
        let t = a.v
    }
)");
  }
  catch (spp::analyse::errors::SppMoveFromBorrowedMemoryError const &e) {
    text = spp_test::StripAnsi(e.what());
  }
  ASSERT_FALSE(text.empty()) << "Expected a move from borrowed memory error.";
  const auto borrowed = text.find("Memory was borrowed here");
  const auto note = text.find("= Note:");
  ASSERT_NE(borrowed, std::string::npos) << text;
  ASSERT_NE(note, std::string::npos) << text;
  EXPECT_LT(borrowed, note) << text;
}
