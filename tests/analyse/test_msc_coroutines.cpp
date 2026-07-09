#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCoroutineSend,
    test_valid_gen_receives_send_value, R"(
    cor c() -> Gen[Str, Bool] {
        let mut recv = gen "hello"
        recv = false
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestCoroutineSend,
    test_invalid_received_send_value_used_as_wrong_type,
    SppTypeMismatchError, R"(
    cor c() -> Gen[Str, Bool] {
        let mut recv = gen "hello"
        recv = "not a bool"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCoroutineSend,
    test_valid_consumer_sends_value, R"(
    cor c() -> Gen[Str, Bool] {
        let recv = gen "hello"
    }

    fun f() -> Void {
        let mut coroutine = c()
        coroutine.res(true)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestCoroutineSend,
    test_invalid_consumer_sends_wrong_type,
    SppFunctionCallNoValidSignaturesError, R"(
    cor c() -> Gen[Str, Bool] {
        let recv = gen "hello"
    }

    fun f() -> Void {
        let mut coroutine = c()
        coroutine.res("wrong")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCoroutineSend,
    test_valid_send_round_trip, R"(
    cor c() -> Gen[Str, Bool] {
        let mut flag = gen "first"
        flag = false
        let flag2 = gen "second"
    }

    fun f() -> Void {
        let mut coroutine = c()
        coroutine.res(true)
        coroutine.res(false)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCoroutineSend,
    test_valid_gen_void_send_no_binding, R"(
    cor c() -> Gen[Str] {
        gen "hello"
    }

    fun f() -> Void {
        let mut coroutine = c()
        coroutine.res()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestCoroutineSend,
    DISABLED_test_invalid_receive_from_void_send,
    SppInvalidVoidValueError, R"(
    cor c() -> Gen[Str] {
        let recv = gen "hello"
    }
)");