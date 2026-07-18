#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_specialization_vector_string, R"(
    sup Vec[Str] {
        !public
        fun test_func(&self) -> S32 {
            ret 1
        }
    }

    sup S32 {
        !public
        fun to_string(&self) -> Str {
            ret Str::from("")
        }
    }

    fun f() -> Void {
        let mut v = Vec[Str]()
        let mut x = v.test_func()
        x = 1234
        v.append(x.to_string())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSpecialization,
    test_specialization_failure_different_generic,
    SppIdentifierUnknownError, R"(
    sup Vec[Str] {
        !public
        fun test_func(&self) -> S32 {
            ret 1
        }
    }

    fun f() -> Void {
        let v = Vec[Bool]()
        let x = v.test_func()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_methods_on_different_generic_names, R"(
    cls MyType[T] {
        !public
        a: T
    }

    sup [T] MyType[T] {
        !public
        fun test_func_0(self) -> T {
            ret self.a
        }
    }

    sup [U] MyType[U] {
        !public
        fun test_func_1(self) -> U {
            ret self.a
        }
    }

    sup S32 {
        !public
        fun to_string(&self) -> Str {
            ret Str::from("")
        }
    }

    fun f() -> Void {
        let t = MyType[S32]()
        let a = t.test_func_0()

        let u = MyType[Str]()
        let mut b = u.test_func_1()

        b = a.to_string()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_blanket_specialization, R"(
    sup [T] T {
        !public
        fun test_func(&self) -> S32 {
            ret 1
        }
    }

    fun f() -> Void {
        let x = 1.test_func()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_constraint_specialization_valid, R"(
    cls MyType[T] { }
    sup [T: Copy] MyType[T] {
        !public
        fun copy_only(&self) -> S32 { ret 1 }
    }

    fun f() -> Void {
        let x = MyType[U32]()
        let mut y = x.copy_only()
        y = 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSpecialization,
    test_constraint_specialization_invalid,
    SppIdentifierUnknownError, R"(
    cls MyType[T] { }
    sup [T: Copy] MyType[T] {
        !public
        fun copy_only(&self) -> S32 { ret 1 }
    }

    fun f() -> Void {
        let x = MyType[Str]()
        let y = x.copy_only()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSpecialization,
    test_partial_specialization_valid, R"(
    cls Pair[A, B] { }
    sup [A] Pair[A, S32] {
        !public
        fun second_is_s32(&self) -> S32 { ret 1 }
    }

    fun f() -> Void {
        let x = Pair[Bool, S32]()
        let mut y = x.second_is_s32()
        y = 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSpecialization,
    test_partial_specialization_invalid,
    SppIdentifierUnknownError, R"(
    cls Pair[A, B] { }
    sup [A] Pair[A, S32] {
        !public
        fun second_is_s32(&self) -> S32 { ret 1 }
    }

    fun f() -> Void {
        let x = Pair[Bool, Bool]()
        let y = x.second_is_s32()
    }
)");
