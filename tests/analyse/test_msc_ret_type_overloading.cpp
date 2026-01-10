#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_assignment, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = "hello world"
        x = g()
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_let_statement, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let x: Bool = g()
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_return_statement, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Bool {
        ret g()
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_gen_expression, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void
    use std::generator::Gen

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }

    cor f() -> Gen[Bool] {
        gen g()
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_complex, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    cls MyType { }

    cls To[Target] { }
    sup [Target] To[Target] {
        @abstract_method
        fun to(&self) -> Target {  }
    }

    sup MyType ext To[Str] {
        fun to(&self) -> Str { ret "" }
    }

    sup MyType ext To[Bool] {
        fun to(&self) -> Bool { ret false }
    }

    fun f() -> Void {
        let mut x = MyType()
        let string: Str = x.to()
        let boolean: Bool = x.to()
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_class_attribute, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    cls MyType {
        a: Bool
    }

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType(a=g())
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_generic_class_attribute_explicit_argument, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    cls MyType[T] {
        a: T
    }

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType[T=Str](a=g())
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestReturnTypeOverloading,
    test_invalid_return_type_overloading_infer_from_generic_class_attribute,
    SppFunctionCallOverloadAmbiguousError, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    cls MyType[T] {
        a: T
    }

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }

    fun f() -> Void {
        let mut x = MyType(a=g())
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestReturnTypeOverloading,
    test_valid_return_type_overloading_infer_from_function_parameter, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }
    fun h(x: Bool) -> Void { }

    fun f() -> Void {
        h(g())
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestReturnTypeOverloading,
    test_invalid_return_type_overloading_infer_from_function_parameter,
    SppFunctionCallOverloadAmbiguousError, R"(
    use std::string::Str
    use std::boolean::Bool
    use std::void::Void

    fun g() -> Str { ret "" }
    fun g() -> Bool { ret false }
    fun h(x: Str) -> Void { }
    fun h(x: Bool) -> Void { }

    fun f() -> Void {
        h(g())
    }
)");;
