#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_valid_memory_mov_iterator_no_modifications, R"(
    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mov() { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_valid_memory_mut_iterator_no_modifications, R"(
    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mut() { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_valid_memory_ref_iterator_no_modifications, R"(
    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_ref() { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_invalid_memory_mov_iterator_modify_owned_object,
    SppUninitializedMemoryUseError, R"(
    fun f(mut v: Vec[StrView]) -> Void {
        loop x in v.iter_mov() {
            v.push_back("hello")
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_invalid_memory_mut_iterator_mut_owned_object,
    SppMemoryOverlapUsageError, R"(
    fun f(mut v: Vec[StrView]) -> Void {
        loop x in v.iter_mut() {
            v.push_back("hello")
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_invalid_memory_mut_iterator_ref_owned_object,
    SppMemoryOverlapUsageError, R"(
    fun f(mut v: Vec[StrView]) -> Void {
        loop x in v.iter_mut() {
            v.push_back("hello")
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_invalid_memory_ref_iterator_mut_owned_object,
    SppMemoryOverlapUsageError, R"(
    fun f(mut v: Vec[StrView]) -> Void {
        loop x in v.iter_ref() {
            v.push_back("hello")
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_valid_memory_mut_iterator_modify_owned_object_after_loop, R"(
    fun f(mut v: Vec[StrView]) -> Void {
        loop x in v.iter_mut() {
        }
        v.push_back("hello")
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_valid_memory_mut_iterator_modify_owned_object_clone, R"(
    fun f(mut v: Vec[StrView]) -> Void {
        loop x in v.clone().iter_mut() {
            v.push_back("hello world")
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_valid_memory_ref_iterator_use_ref, R"(
    fun f(mut v: Vec[StrView]) -> Void {
        loop x in v.iter_ref() {
            let l = v.length()
        }
    }
)");
