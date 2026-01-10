#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_let,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let x = "hello world"
        let coroutine = g(&x)
        let y = x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_assign_variable,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    fun f() -> std::void::Void {
        let y: std::string::Str
        let x = "hello world"
        let coroutine = g(&x)
        y = x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_assign_attribute,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    cls A {
        a: std::string::Str
    }

    fun f(mut a: A) -> std::void::Void {
        let y: std::string::Str
        let x = "hello world"
        let coroutine = g(&x)
        a.a = x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_func_call,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    fun f(x: std::string::Str) -> std::void::Void { }

    fun h() -> std::void::Void {
        let x = "hello world"
        let coroutine = g(&x)
        f(x)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_object_init,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    cls A {
        a: std::string::Str
    }

    fun h() -> std::void::Void {
        let x = "hello world"
        let coroutine = g(&x)
        let a = A(a=x)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_inner_scope_return,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    fun h() -> std::void::Void {
        let y = {
            let x = "hello world"
            let coroutine = g(&x)
            x
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_loop_escape,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    fun h() -> std::void::Void {
        loop true {
            let x = "hello world"
            let coroutine = g(&x)
            exit x
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_gen,
    SppMoveFromPinnedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    cor h() -> std::generator::Gen[std::string::Str, std::boolean::Bool] {
        let x = "hello world"
        let coroutine = g(&x)
        gen x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPins,
    test_invalid_moving_pinned_borrow_ret,
    SppMoveFromPinLinkedMemoryError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str, std::boolean::Bool] { }

    fun h() -> std::generator::Gen[std::string::Str, std::boolean::Bool] {
        let x = "hello world"
        let coroutine = g(&x)
        ret coroutine
    }
)");
