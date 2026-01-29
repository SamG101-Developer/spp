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

#include <spp/macros.hpp>
#include <spp/parse/macros.hpp>
#include <gtest/gtest.h>
#include "test_boot.hpp"


#define SPP_TEST_SHOULD_PASS_SYNTACTIC(name, code) \
    TEST(SppParser, name) {                        \
        auto ast = INJECT_CODE(code, parse);       \
    }


#define SPP_TEST_SHOULD_FAIL_SYNTACTIC(name, code)                                  \
    TEST(SppParser, name) {                                                         \
        EXPECT_THROW(INJECT_CODE(code, parse), spp::parse::errors::SppSyntaxError); \
    }


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
