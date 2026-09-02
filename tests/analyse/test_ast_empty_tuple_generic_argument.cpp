#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyTupleGenericArgument,
  test_valid_empty_tuple_as_a_generic_argument, R"(
    cls Holder[T] { }

    fun f() -> Void {
        let h = Holder[()]()
        std::mem::ops::drop(h)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyTupleGenericArgument,
  test_valid_destructor_for_a_class_holding_an_empty_tuple, R"(
    cls Holder[T] { }

    sup [T] Holder[T] ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Self(..) = self
        }
    }

    fun f() -> Void {
        std::mem::ops::drop(Holder[()]())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyTupleGenericArgument,
  test_valid_destructor_for_a_class_holding_a_non_empty_tuple, R"(
    cls Holder[T] { }

    sup [T] Holder[T] ext std::ops::drop::Drop {
        fun drop(self) -> Void {
            let Self(..) = self
        }
    }

    fun f() -> Void {
        std::mem::ops::drop(Holder[(S32, S32)]())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyTupleGenericArgument,
  test_valid_empty_tuple_in_a_function_type, R"(
    fun g[F: std::function::FunMov[(), Void]](f: F) -> Void {
        f()
    }

    fun f() -> Void {
        g(() -> Void { })
    }
)");
