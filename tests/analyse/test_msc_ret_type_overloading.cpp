#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_assignment, R"(
    fun g() -> StrView { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = "hello world"
        x = g()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_let_statement, R"(
    fun g() -> StrView { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let x: Bool = g()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_return_statement, R"(
    fun g() -> StrView { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Bool {
        ret g()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_gen_expression, R"(
    fun g() -> StrView { ret "" }
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
        fun to(&self) -> Target {  }
    }

    sup MyType ext To[StrView] {
        fun to(&self) -> StrView { ret "" }
    }

    sup MyType ext To[Bool] {
        fun to(&self) -> Bool { ret false }
    }

    fun f() -> Void {
        let mut x = MyType()
        let string: StrView = x.to()
        let boolean: Bool = x.to()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_class_attribute, R"(
    cls MyType {
        a: Bool
    }

    fun g() -> StrView { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType(a=g())
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_generic_class_attribute_explicit_argument, R"(
    cls MyType[T] {
        a: T
    }

    fun g() -> StrView { ret "" }
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
        a: T
    }

    fun g() -> StrView { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType(a=g())
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_function_parameter, R"(
    fun g() -> StrView { ret "" }
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
    fun g() -> StrView { ret "" }
    fun g() -> Bool { ret false }
    fun h(x: StrView) -> Void { }
    fun h(x: Bool) -> Void { }

    fun f() -> Void {
        h(g())
    }
)");
