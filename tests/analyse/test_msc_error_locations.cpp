#include "../test_macros.hpp"

// An error whose blocks show code from more than one file is raised with a scope for each block, in order: one scope
// formats every block alike, so a declaration in another file (here, std) is shown against the test's own file - past
// its end, or at the wrong line. Each case uses a std declaration from "main.spp", so the failing check is the
// location one ("CheckErrorLocations"). A call's own errors are raised inside overload resolution, so they reach the
// test as "no valid signatures", which shows each candidate's error with its blocks.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestErrorLocations,
    test_invalid_function_argument_name_declared_in_another_file,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void { let x = std::process::get_env(nope="PATH") }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestErrorLocations,
    test_invalid_function_argument_missing_declared_in_another_file,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void { let x = std::process::get_env() }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestErrorLocations,
    test_invalid_too_many_function_arguments_declared_in_another_file,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void { let x = std::process::get_env("PATH", "HOME") }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestErrorLocations,
    test_invalid_attribute_type_declared_in_another_file,
    SppTypeMismatchError, R"(
    fun f() -> Void { let p = std::tuple::Pair[S32, Bool](key=true, val=false) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestErrorLocations,
    test_invalid_destructure_missing_attribute_declared_in_another_file,
    SppArgumentMissingError, R"(
    fun f() -> Void {
        let p = std::tuple::Pair[S32, Bool](key=1_s32, val=false)
        let std::tuple::Pair[S32, Bool](key) = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestErrorLocations,
    test_invalid_too_many_generic_arguments_declared_in_another_file,
    SppGenericArgumentTooManyError, R"(
    fun f() -> Void { let p = std::tuple::Pair[S32, Bool, Str](key=1_s32, val=false) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestErrorLocations,
    test_invalid_generic_parameter_conflict_declared_in_another_file,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        let mut a = 1_s32
        let mut b = false
        std::mem::ops::swap(&mut a, &mut b)
    }
)");
