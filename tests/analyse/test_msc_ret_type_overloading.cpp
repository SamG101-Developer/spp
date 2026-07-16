#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_assignment, R"(
    fun g() -> Str { ret Str::from("") }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        x = g()
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
        let string: StrView = x.into()
        let boolean: Bool = x.into()
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
    }
)");

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
        let mut x = MyType[T=StrView](a=g())
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
    }
)");

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
