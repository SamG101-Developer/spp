#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_partial_move_from_yielded_borrow_via_variable,
    SppMoveFromBorrowedMemoryError, R"(
    cls A {
        !public a: Str
    }

    cor g() -> Gen[&A] { }

    fun f() -> Void {
        let mut generator = g()
        let a = generator.res()
        let b = case a of {
            is &A(..) { a.a }
            else { Str::from("nothing") }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_partial_move_from_yielded_borrow_directly,
    SppMoveFromBorrowedMemoryError, R"(
    cls A {
        !public a: Str
    }

    cor g() -> Gen[&A] { }

    fun f() -> Void {
        let mut generator = g()
        let b = case generator.res() of {
            is &A(a) { a }
            else { Str::from("nothing") }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_use_mut_borrow_after_conflicting_ref_borrow_created_simple,
    SppMemoryOverlapUsageError, R"(
    cor g(a: &Str) -> Gen[Str] { }

    fun h(a: &mut Str) -> Void { }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        let mut coroutine = g(&x)  # take an immutable borrow
        h(&mut x)                  # conflicting mutable borrow
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_use_ref_borrow_after_conflicting_mut_borrow_created_simple,
    SppMemoryOverlapUsageError, R"(
    cor g(a: &mut Str) -> Gen[Str] { }

    fun h(a: &Str) -> Void { }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        let mut coroutine = g(&mut x)  # take a mutable borrow
        h(&x)                          # conflicting immutable
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_use_mut_borrow_after_conflicting_mut_borrow_created_simple,
    SppMemoryOverlapUsageError, R"(
    cor g(a: &mut Str) -> Gen[Str] { }

    fun h(a: &mut Str) -> Void { }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        let mut coroutine = g(&mut x)  # take a mutable borrow
        h(&mut x)                      # conflicting mutable borrow
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_ref_borrow_after_conflicting_ref_borrow_created_simple, R"(
    cor g(a: &Str) -> Gen[Str] { }

    fun h(a: &Str) -> Void { }

    fun f() -> Void {
        let x = Str::from("hello world")
        let mut coroutine = g(&x)  # take an immutable borrow
        h(&x)                      # conflicting immutable borrow does not invalidate the first immutable borrow
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_define_conflicting_mut_borrow_after_mut_borrow_created,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_mut_1 = object.custom_iter_mut()
        let generator_mut_2 = object.custom_iter_mut()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_define_conflicting_ref_borrow_after_mut_borrow_created,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_mut = object.custom_iter_mut()
        let generator_ref = object.custom_iter_ref()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_define_conflicting_mut_borrow_after_ref_borrow_created,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_ref = object.custom_iter_ref()
        let generator_mut = object.custom_iter_mut()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_define_conflicting_ref_borrow_after_ref_borrow_created, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_ref_1 = object.custom_iter_ref()
        let generator_ref_2 = object.custom_iter_ref()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_ref_borrow_use_mut_borrow,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let mut generator_mut = object.custom_iter_mut()
        let generator_ref = object.custom_iter_ref()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_ref_borrow_create_mut_borrow_use_ref_borrow,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_ref = object.custom_iter_ref()
        let mut generator_mut = object.custom_iter_mut()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_mut_borrow_use_mut_borrow,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let mut generator_mut_1 = object.custom_iter_mut()
        let mut generator_mut_2 = object.custom_iter_mut()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_ref_borrow_use_mut_borrow_with_scoping,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_mut: Gen[&mut Str, Void]
        case true {
            generator_mut = object.custom_iter_mut()
        }
        let generator_ref = object.custom_iter_ref()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_ref_borrow_create_mut_borrow_use_ref_borrow_with_scoping,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_ref: Gen[&Str, Void]
        case true {
            generator_ref = object.custom_iter_ref()
        }
        let mut generator_mut = object.custom_iter_mut()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_mut_borrow_use_mut_borrow_with_scoping,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let generator_mut_1: Gen[&mut Str, Void]
        case true {
            generator_mut_1 = object.custom_iter_mut()
        }
        let mut generator_mut_2 = object.custom_iter_mut()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_create_ref_borrow_create_ref_borrow_use_ref_borrow_with_scoping, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let object = MyType()
        let mut generator_ref_1: Gen[&Str, Void]
        case true {
            generator_ref_1 = object.custom_iter_ref()
        }
        let generator_ref_2 = object.custom_iter_ref()
        generator_ref_1.res()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_use_mut_borrow_after_conflicting_mut_borrow_created_for_resume,
    SppUninitializedMemoryUseError, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let mut generator_mut = object.custom_iter_mut()
        let x = generator_mut.res()
        let y = generator_mut.res()
        let z = case x of {
            is &mut Str(..) { x.to_uppercase() }
            else { Str::from("") }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_ref_borrow_after_conflicting_ref_borrow_created_for_resume, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        let mut generator_ref = object.custom_iter_ref()
        let x = generator_ref.res()
        let y = generator_ref.res()
        let z = case x of {
            is &Str(..) { x.to_uppercase() }
            else { Str::from("") }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_mut_borrow_after_conflicting_ref_borrow_created_with_scoping, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        loop true {
            let generator_mut = object.custom_iter_mut()
        }
        let mut generator_ref = object.custom_iter_ref()
        generator_ref.res()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_ref_borrow_after_conflicting_mut_borrow_created_with_scoping, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        loop true {
            let generator_ref = object.custom_iter_ref()
        }
        let mut generator_mut = object.custom_iter_mut()
        generator_mut.res()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_mut_borrow_after_conflicting_mut_borrow_created_with_scoping, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let mut object = MyType()
        loop true {
            let generator_mut_1 = object.custom_iter_mut()
        }
        let mut generator_mut_1 = object.custom_iter_mut()
        generator_mut_1.res()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_ref_borrow_after_conflicting_ref_borrow_created_with_scoping_2, R"(
    cls MyType { }
    sup MyType {
        !public cor custom_iter_ref(&self) -> Gen[&Str, Void] { }
        !public cor custom_iter_mut(&mut self) -> Gen[&mut Str, Void] { }
    }

    fun test() -> Void {
        let object = MyType()
        loop true {
            let generator_ref_1 = object.custom_iter_ref()
        }
        let mut generator_ref_2 = object.custom_iter_ref()
        generator_ref_2.res()
    }
)");
