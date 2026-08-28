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

#define SPP_TEST_SHOULD_PASS_SEMANTIC(group, name, code) \
    TEST(group, name) {                                  \
        build_temp_project(code);                        \
    }

#define SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(group, name, code) \
    TEST(group, name) {                                          \
        build_temp_project(code, false);                         \
    }

#define SPP_TEST_SHOULD_FAIL_SEMANTIC(group, name, error, code)              \
    TEST(group, name) {                                                      \
        EXPECT_THROW(build_temp_project(code), spp::analyse::errors::error); \
    }

#define SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(group, name, error, code)             \
    TEST(group, name) {                                                             \
        EXPECT_THROW(build_temp_project(code, false), spp::analyse::errors::error); \
    }

#define SPP_TEST_CMP_VALUES(group, name, code, ...)                                          \
    TEST(group, name) {                                                                      \
        const auto actual = build_temp_project(code);                                         \
        for (auto const &[key, expected] : std::map<spp::Str, spp::Str>{__VA_ARGS__}) {      \
            const auto it = actual.find(key);                                                 \
            if (it == actual.end()) {                                                         \
                ADD_FAILURE() << "no compile-time constant named '" << key << "'";            \
                continue;                                                                     \
            }                                                                                 \
            EXPECT_EQ(it->second, expected) << "compile-time constant '" << key << "'";       \
        }                                                                                     \
    }
