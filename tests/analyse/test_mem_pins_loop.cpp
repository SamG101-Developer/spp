#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_mov_iterator_no_modifications, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mov() { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_mut_iterator_no_modifications, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mut() { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_ref_iterator_no_modifications, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_ref() { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_mov_iterator_modify_owned_object,
    SppUninitializedMemoryUseError, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mov() {
            v.push("hello")
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_mut_iterator_mut_owned_object,
    SppMemoryOverlapUsageError, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mut() {
            v.push("hello")
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_mut_iterator_ref_owned_object,
    SppMemoryOverlapUsageError, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mut() {
            v.push("hello")
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_ref_iterator_mut_owned_object,
    SppMemoryOverlapUsageError, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_ref() {
            v.push("hello")
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_mut_iterator_modify_owned_object_after_loop, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_mut() {
        }
        v.push("hello")
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_mut_iterator_modify_owned_object_clone, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.clone().iter_mut() {
            v.push("hello world")
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPinsLoop,
    test_ref_iterator_use_ref, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void

    fun f(mut v: Vec[Str]) -> Void {
        loop x in v.iter_ref() {
            let l = v.length()
        }
    }
)");
