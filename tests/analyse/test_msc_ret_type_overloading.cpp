#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_assignment, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        x = g()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_let_statement, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let x: Bool = g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_return_statement, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Bool {
        ret g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_gen_expression, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    cor f() -> Gen[Bool] {
        gen g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_complex, R"(
    cls MyType { }

    cls To[Target] { }
    sup [Target] To[Target] {
        !abstract_method
        !public
        fun into(&self) -> Target { }
    }

    sup MyType ext To[Str] {
        fun into(&self) -> Str { ret Str::from("") }
    }

    sup MyType ext To[Bool] {
        fun into(&self) -> Bool { ret false }
    }

    fun f() -> Void {
        let mut x = MyType()
        let string: Str = x.into()
        let boolean: Bool = x.into()
        drop(string)
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_class_attribute, R"(
    cls MyType {
        !public
        a: Bool
    }

    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType(a=g())
        std::mem::ops::drop(x)
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_generic_class_attribute_explicit_argument, R"(
    cls MyType[T] {
        !public
        a: T
    }

    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType[T=Str](a=g())
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestReturnTypeOverloading,
  test_invalid_return_type_overloading_infer_from_generic_class_attribute,
  SppFunctionCallOverloadAmbiguousError, R"(
    cls MyType[T] {
        !public
        a: T
    }

    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType(a=g())
        std::mem::ops::drop(x)
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_function_parameter, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun h(x: Bool) -> Void { }

    fun f() -> Void {
        h(g())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestReturnTypeOverloading,
  test_invalid_return_type_overloading_infer_from_function_parameter,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun h(x: &StrView) -> Void { }
    fun h(x: Bool) -> Void { }

    fun f() -> Void {
        h(g())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestReturnTypeOverloading,
  test_invalid_return_type_overloading_no_context,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let x = g()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestReturnTypeOverloading,
  test_invalid_return_type_overloading_target_matches_no_overload,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let x: S32 = g()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestReturnTypeOverloading,
  test_invalid_return_type_overloading_infer_from_generic_function_parameter,
  SppFunctionCallOverloadAmbiguousError, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun h[T](x: T) -> Void { std::mem::ops::drop(x) }
    fun f() -> Void { h(g()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_keyword_function_argument, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun h(a: S32, x: Bool) -> Void { }
    fun f() -> Void { h(a=1, x=g()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_second_function_parameter, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun h(a: S32, x: Bool) -> Void { }
    fun f() -> Void { h(1, g()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_function_overloads_that_agree, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun h(x: Bool) -> Void { }
    fun h(x: Bool, y: S32) -> Void { }
    fun f() -> Void { h(g()) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_method_parameter, R"(
    cls K { }
    sup K { !public fun take(&self, x: Bool) -> Void { } }
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun f() -> Void {
        let k = K()
        k.take(g())
        std::mem::ops::drop(k)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_through_nested_calls, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun h(x: Bool) -> Bool { ret x }
    fun f() -> Void { let y = h(h(g())) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_method_of_a_generic_receiver, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun f() -> Void {
        let mut v = Vec[Bool]::new()
        v.append(g())
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_generic_class_attribute_positional_argument, R"(
    cls MyType[T] {
        !public
        a: T
    }
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun f() -> Void { std::mem::ops::drop(MyType[Str](a=g())) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestReturnTypeOverloading,
  test_valid_return_type_overloading_infer_from_generic_class_attribute_inside_a_type, R"(
    cls MyType[T] {
        !public
        a: Opt[T]
    }
    fun g() -> Opt[Str] { ret None() }
    fun g() -> Bool { ret false }
    fun f() -> Void { std::mem::ops::drop(MyType[T=Str](a=g())) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestReturnTypeOverloading,
  test_invalid_return_type_overloading_infer_from_partially_bound_generic_class_attribute,
  SppFunctionCallOverloadAmbiguousError, R"(
    cls MyType[T, U] {
        !public
        a: T
        !public
        b: U
    }
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }
    fun f() -> Void { std::mem::ops::drop(MyType[T=Str](a=Str::from("x"), b=g())) }
)");
