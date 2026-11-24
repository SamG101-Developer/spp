#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropParameters,
    test_valid_move_parameter_returned, R"(
    cls Handle { !public fd: S32 }

    fun pass_on(h: Handle) -> Handle {
        ret h
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropParameters,
    test_valid_move_parameter_taken_apart, R"(
    cls Handle { !public fd: S32 }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropParameters,
    test_valid_borrowed_self_not_consumed, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        fun peek(&self) -> S32 {
            ret self.fd
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropParameters,
  test_valid_move_parameter_not_consumed, R"(
    cls Handle { !public fd: S32 }

    fun ignore(h: Handle) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropParameters,
  test_valid_move_self_not_consumed, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        fun close(self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDropParameters,
    test_valid_move_self_taken_apart, R"(
    cls Handle { !public fd: S32 }

    sup Handle {
        fun close(self) -> S32 {
            let Handle(fd) = self
            ret fd
        }
    }
)");
