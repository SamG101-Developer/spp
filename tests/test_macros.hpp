#pragma once
#define GTEST_BUILD_WITH_IMPORT_STD

import pthread;
import sys;
using pthread::pthread_create;
using pthread::pthread_equal;
using pthread::pthread_getspecific;
using pthread::pthread_join;
using pthread::pthread_key_create;
using pthread::pthread_key_delete;
using pthread::pthread_self;
using pthread::pthread_mutex_destroy;
using pthread::pthread_mutex_init;
using pthread::pthread_mutex_lock;
using pthread::pthread_mutex_unlock;
using pthread::pthread_setspecific;
using pthread::pthread_t;
using pthread::pthread_key_t;
using pthread::pthread_mutex_t;
using sys::chdir;
using sys::close;
using sys::errno;
using sys::fileno;
using sys::fdopen;
using sys::isatty;
using sys::read;
using sys::rmdir;
using sys::strcasecmp;
using sys::stderr;
using sys::stdin;
using sys::stdout;
using sys::write;
using sys::stat;
using sys::S_ISDIR;

#include <gtest/gtest.h>
#include <spp/macros.hpp>
#include "test_boot.hpp"

// Named by the "SHOULD_FAIL_SEMANTIC" macros below, so every test file needs it. The parser and lexer are not here:
// only the syntactic macros use them, only one test file uses those, and importing them here made the other ~120
// translation units build the parser to run a semantic test. See "test_macros_parse.hpp".
import spp.analyse.errors.semantic_error;

namespace spp_test {
  /// The error formatter colours its output; strip the escapes
  /// so a block can be read as plain text.
  inline auto StripAnsi(std::string const &s) -> std::string {
    auto out = std::string();
    out.reserve(s.size());
    for (auto i = 0uz; i < s.size(); ++i) {
      if (s[i] == '\x1b' and i + 1 < s.size() and s[i + 1] == '[') {
        while (i < s.size() and s[i] != 'm') { ++i; }
        continue;
      }
      out += s[i];
    }
    return out;
  }

  /// The text a block's carets underline: the characters of its
  /// quoted source line that sit above a "^". The source row is
  /// "N | code" and the caret row below it lines up column for
  /// column. Empty when the block quotes no source.
  inline auto Underlined(std::string const &block) -> std::string {
    auto lines = std::vector<std::string>();
    for (auto start = 0uz; start <= block.size();) {
      const auto end = block.find('\n', start);
      lines.push_back(block.substr(start, end == std::string::npos ? std::string::npos : end - start));
      if (end == std::string::npos) { break; }
      start = end + 1;
    }
    for (auto i = 0uz; i + 1 < lines.size(); ++i) {
      auto const &code = lines[i];
      auto const &carets = lines[i + 1];
      const auto bar = code.find(" | ");
      if (bar == std::string::npos or carets.find('^') == std::string::npos) { continue; }
      const auto number = code.substr(0, bar);
      if (number.find_first_not_of(' ') == std::string::npos
        or number.find_first_not_of(" 0123456789") != std::string::npos) { continue; }
      auto out = std::string();
      for (auto c = 0uz; c < carets.size() and c < code.size(); ++c) {
        if (carets[c] == '^') { out += code[c]; }
      }
      return out;
    }
    return {};
  }

  /// Check where a raised error points. No block may show generated
  /// code or the end of the file, and every primary ("Error in")
  /// block must be in the test's own "main.spp". Overload
  /// candidates listed under the error are skipped: they point at
  /// the candidates' definitions, wherever those are. With
  /// "underline" set, the first primary block must underline
  /// exactly that text.
  template <typename Messages>
  auto CheckErrorLocations(Messages const &messages, char const *underline) -> void {
    auto primary_seen = false;
    for (auto const &raw : messages) {
      const auto block = StripAnsi(std::string(raw));
      if (block.starts_with("-----")) { continue; }
      const auto is_error = block.find("Error in file '") != std::string::npos;
      const auto is_context = block.find("Context from file '") != std::string::npos;
      if (not is_error and not is_context) { continue; }

      EXPECT_EQ(block.find("<generated code>"), std::string::npos)
        << "An error block points at generated code:\n" << block;
      EXPECT_EQ(block.find("<end of file>"), std::string::npos)
        << "An error block points past the end of the file:\n" << block;
      if (not is_error) { continue; }

      const auto q1 = block.find('\'');
      const auto q2 = block.find('\'', q1 + 1);
      const auto path = block.substr(q1 + 1, q2 - q1 - 1);
      EXPECT_TRUE(path.ends_with("/src/main.spp"))
        << "An error block points outside the test's own source:\n" << block;
      if (underline != nullptr and not primary_seen) {
        EXPECT_EQ(Underlined(block), std::string(underline))
          << "The error block underlines the wrong code:\n" << block;
      }
      primary_seen = true;
    }
    if (underline != nullptr and not primary_seen) {
      ADD_FAILURE() << "The error has no primary block to check the underline of.";
    }
  }

  /// Compile "code", expect it to raise "E", and check where the
  /// error points (see "CheckErrorLocations").
  template <typename E>
  auto ExpectSemanticError(
    std::string code, const bool add_main, char const *error_name, char const *underline) -> void {
    try {
      build_temp_project(std::move(code), add_main);
    }
    catch (E const &e) {
      CheckErrorLocations(e.messages, underline);
      return;
    }
    catch (std::exception const &e) {
      ADD_FAILURE() << "Expected " << error_name << ", but a different error was thrown:\n" << e.what();
      return;
    }
    ADD_FAILURE() << "Expected " << error_name << ", but nothing was thrown.";
  }
}

#define SPP_TEST_SHOULD_PASS_SEMANTIC(group, name, code) \
    TEST(group, name) {                                  \
        build_temp_project(code);                        \
    }

#define SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(group, name, code) \
    TEST(group, name) {                                          \
        build_temp_project(code, false);                         \
    }

#define SPP_TEST_SHOULD_FAIL_SEMANTIC(group, name, error, code)                                   \
    TEST(group, name) {                                                                           \
        spp_test::ExpectSemanticError<spp::analyse::errors::error>(code, true, #error, nullptr); \
    }

#define SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(group, name, error, code)                            \
    TEST(group, name) {                                                                            \
        spp_test::ExpectSemanticError<spp::analyse::errors::error>(code, false, #error, nullptr); \
    }

#define SPP_TEST_SHOULD_FAIL_SEMANTIC_AT(group, name, error, underline, code)                       \
    TEST(group, name) {                                                                             \
        spp_test::ExpectSemanticError<spp::analyse::errors::error>(code, true, #error, underline); \
    }

#define SPP_TEST_CMP_VALUES(group, name, code, ...)                                      \
    TEST(group, name) {                                                                  \
        const auto actual = build_temp_project(code);                                    \
        for (auto const &[key, expected] : spp::Map<spp::Str, spp::Str>{__VA_ARGS__}) {  \
            const auto it = actual.find(key);                                            \
            if (it == actual.end()) {                                                    \
                ADD_FAILURE() << "no compile-time constant named '" << key << "'";       \
                continue;                                                                \
            }                                                                            \
            EXPECT_EQ(it->second, expected) << "compile-time constant '" << key << "'";  \
        }                                                                                \
    }
