#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_let_statement_explicit_type, R"(
    cls A { }
    sup A {
        !public fun m(&self) -> Void {
            let x: Self = A()
            std::mem::ops::drop(x)
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_uninitialized_let_statement_type, R"(
    cls A { }
    sup A {
        !public fun m(&self) -> Void {
            let x: Self
            x = A()
            std::mem::ops::drop(x)
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_inside_a_parameter_type, R"(
    cls A { }
    sup A {
        !public fun m(&self, x: Vec[Self]) -> Void { std::mem::ops::drop(x) }
    }
    fun f() -> Void {
        let a = A()
        a.m(Vec[A]())
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_inside_a_class_attribute_type, R"(
    cls A {
        !public
        v: Vec[Self]
    }
    fun f() -> Void {
        let a = A(v=Vec[A]())
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_inside_a_type_alias_targets_generic_arguments, R"(
    cls A { }
    sup A {
        type Mine = Vec[Self]
        !public fun m(&self) -> Void {
            let x: Mine = Vec[A]()
            std::mem::ops::drop(x)
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_type_alias_target, R"(
    cls A { }
    sup A {
        type Mine = Self
        !public fun m(&self) -> Void {
            let x: Mine = A()
            std::mem::ops::drop(x)
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_inside_a_tuple_type, R"(
    cls A { }
    sup A {
        !public fun m(&self) -> Void {
            let x: (Self, Self) = (A(), A())
            std::mem::ops::drop(x)
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_inside_a_variant_type, R"(
    cls A { }
    sup A {
        !public fun m(&self) -> Void {
            let x: Self or S32 = A()
            std::mem::ops::drop(x)
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_a_generic_argument, R"(
    cls A { }
    sup A {
        !public fun m(&self) -> Void {
            let v = Vec[Self]()
            std::mem::ops::drop(v)
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_a_variadic_parameter_type, R"(
    cls A { }
    sup A {
        !public fun m(&self, ..rest: Self) -> Void { std::mem::ops::drop(rest) }
    }
    fun f() -> Void {
        let a = A()
        let b = A()
        a.m(b)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_an_optional_parameter_default_type, R"(
    cls A { }
    sup A {
        !public fun make() -> Self { ret Self() }
        !public fun m(&self, other: Self = A::make()) -> Void { std::mem::ops::drop(other) }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_an_object_destructure_type, R"(
    cls A { !public n: S32 }
    sup A {
        !public fun m(self) -> S32 {
            let Self(n) = self
            ret n
        }
    }
    fun f() -> Void {
        let a = A(n=1)
        let r = a.m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_an_object_initializer_type, R"(
    cls A { !public n: S32 }
    sup A {
        !public fun make() -> Self { ret Self(n=1) }
    }
    fun f() -> Void {
        let a = A::make()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_a_closure_return_type, R"(
    cls A { }
    sup A {
        !public fun m(&self) -> A {
            let c = () -> Self { ret A() }
            ret c()
        }
    }
    fun f() -> Void {
        let a = A()
        let b = a.m()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositions,
  test_valid_self_as_a_closure_parameter_type, R"(
    cls A { }
    sup A {
        !public fun m(&self) -> Void {
            let c = (x: Self) -> Void { std::mem::ops::drop(x) }
            c(A())
        }
    }
    fun f() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");
