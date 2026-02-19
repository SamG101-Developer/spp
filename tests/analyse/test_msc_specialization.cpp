#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_specialization_vector_string, R"(
    sup std::vector::Vec[std::string::Str] {
        fun test_func(&self) -> std::number::S32 {
            ret 1
        }
    }

    use std::vector::Vec
    use std::string::Str

    sup std::number::S32 {
        fun to_string(&self) -> std::string::Str {
            ret std::string::Str::from("")
        }
    }

    fun f() -> std::void::Void {
        let mut v = Vec[Str]()
        let mut x = v.test_func()
        x = 1234
        v.push(x.to_string())
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSpecialization,
    test_specialization_failure_different_generic,
    SppIdentifierUnknownError, R"(
    sup std::vector::Vec[std::string::Str] {
        fun test_func(&self) -> std::number::S32 {
            ret 1
        }
    }

    use std::vector::Vec
    use std::boolean::Bool

    fun f() -> std::void::Void {
        let v = Vec[Bool]()
        let x = v.test_func()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_methods_on_different_generic_names, R"(
    cls MyType[T] {
        a: T
    }

    sup [T] MyType[T] {
        fun test_func_0(self) -> T {
            ret self.a
        }
    }

    sup [U] MyType[U] {
        fun test_func_1(self) -> U {
            ret self.a
        }
    }

    sup std::number::S32 {
        fun to_string(&self) -> std::string::Str {
            ret std::string::Str::from("")
        }
    }

    fun f() -> std::void::Void {
        let t = MyType[std::number::S32]()
        let a = t.test_func_0()

        let u = MyType[std::string::Str]()
        let mut b = u.test_func_1()

        b = a.to_string()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_blanket_specialization, R"(
    sup [T] T {
        fun test_func(&self) -> std::number::S32 {
            ret 1
        }
    }

    fun f() -> std::void::Void {
        let x = 1.test_func()
    }
)");
