#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_partial_move_from_yielded_borrow_via_variable,
    SppMoveFromBorrowedMemoryError, R"(
    use std::string::Str

    cls A {
        a: Str
    }

    cor g() -> std::generator::Gen[&A] { }

    fun f() -> std::void::Void {
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
    use std::option::None

    cls A {
        a: Str
    }

    cor g() -> std::generator::Gen[&A] { }

    fun f() -> std::void::Void {
        let mut generator = g()
        let b = case generator.res() of {
            is &A(..) { Str::from("smth") }
            else { Str::from("nothing") }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_use_mut_borrow_after_conflicting_ref_borrow_created_simple,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str
    use std::void::Void

    cor g(a: &Str) -> std::generator::Gen[Str] { }

    fun h(a: &mut Str) -> Void { }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        let mut coroutine = g(&x)  # take an immutable borrow
        h(&mut x)                  # conflicting mutable borrow invalidates the immutable borrow
        coroutine.res()            # use of the invalidated immutable borrow
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_use_ref_borrow_after_conflicting_mut_borrow_created_simple,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str
    use std::void::Void

    cor g(a: &mut Str) -> std::generator::Gen[Str] { }

    fun h(a: &Str) -> Void { }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        let mut coroutine = g(&mut x)  # take a mutable borrow
        h(&x)                          # conflicting immutable borrow invalidates the mutable borrow
        coroutine.res()                # use of the invalidated mutable borrow
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_use_mut_borrow_after_conflicting_mut_borrow_created_simple,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str
    use std::void::Void

    cor g(a: &mut Str) -> std::generator::Gen[Str] { }

    fun h(a: &mut Str) -> Void { }

    fun f() -> Void {
        let mut x = Str::from("hello world")
        let mut coroutine = g(&mut x)  # take a mutable borrow
        h(&mut x)                      # conflicting mutable borrow invalidates the first mutable borrow
        coroutine.res()                # use of the invalidated mutable borrow
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_ref_borrow_after_conflicting_ref_borrow_created_simple, R"(
    use std::string::Str
    use std::void::Void

    cor g(a: &Str) -> std::generator::Gen[Str] { }

    fun h(a: &Str) -> Void { }

    fun f() -> Void {
        let x = Str::from("hello world")
        let mut coroutine = g(&x)  # take an immutable borrow
        h(&x)                      # conflicting immutable borrow does not invalidate the first immutable borrow
        coroutine.res()            # use of the valid immutable borrow
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_define_conflicting_mut_borrow_after_mut_borrow_created, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let object = MyType()
        let generator_mut_1 = object.custom_iter_mut()
        let generator_mut_2 = object.custom_iter_mut()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_define_conflicting_ref_borrow_after_mut_borrow_created, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let generator_mut = object.custom_iter_mut()
        let generator_ref = object.custom_iter_ref()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_define_conflicting_mut_borrow_after_ref_borrow_created, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
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
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let generator_ref_1 = object.custom_iter_ref()
        let generator_ref_2 = object.custom_iter_ref()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_ref_borrow_use_mut_borrow,
    SppUninitializedMemoryUseError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_mut = object.custom_iter_mut()
        let generator_ref = object.custom_iter_ref()
        let x = generator_mut.res()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_ref_borrow_create_mut_borrow_use_ref_borrow,
    SppUninitializedMemoryUseError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let generator_ref = object.custom_iter_ref()
        let mut generator_mut = object.custom_iter_mut()
        let x = generator_ref.res()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_mut_borrow_use_mut_borrow,
    SppUninitializedMemoryUseError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_mut_1 = object.custom_iter_mut()
        let mut generator_mut_2 = object.custom_iter_mut()
        let x = generator_mut_1.res()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_ref_borrow_use_mut_borrow_with_scoping,
    SppUninitializedMemoryUseError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let generator_mut: std::generator::Gen[&mut std::string::Str, std::void::Void]
        case true {
            generator_mut = object.custom_iter_mut()
        }
        let generator_ref = object.custom_iter_ref()
        generator_mut.res()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_ref_borrow_create_mut_borrow_use_ref_borrow_with_scoping,
    SppUninitializedMemoryUseError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let generator_ref: std::generator::Gen[&std::string::Str, std::void::Void]
        case true {
            generator_ref = object.custom_iter_ref()
        }
        let mut generator_mut = object.custom_iter_mut()
        generator_ref.res()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_memory_create_mut_borrow_create_mut_borrow_use_mut_borrow_with_scoping,
    SppUninitializedMemoryUseError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let generator_mut_1: std::generator::Gen[&mut std::string::Str, std::void::Void]
        case true {
            generator_mut_1 = object.custom_iter_mut()
        }
        let mut generator_mut_2 = object.custom_iter_mut()
        generator_mut_1.res()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_create_ref_borrow_create_ref_borrow_use_ref_borrow_with_scoping, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let object = MyType()
        let mut generator_ref_1: std::generator::Gen[&std::string::Str, std::void::Void]
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
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_mut = object.custom_iter_mut()
        let x = generator_mut.res()
        let y = generator_mut.res()
        let z = case x of {
            is &mut std::string::Str(..) { x.to_ascii_uppercase() }
            else { std::string::Str::from("") }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_ref_borrow_after_conflicting_ref_borrow_created_for_resume, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_ref = object.custom_iter_ref()
        let x = generator_ref.res()
        let y = generator_ref.res()
        let z = case x of {
            is &std::string::Str(..) { x.to_ascii_uppercase() }
            else { std::string::Str::from("") }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_memory_use_mut_borrow_after_conflicting_ref_borrow_created_with_scoping, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
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
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
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
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
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
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let object = MyType()
        loop true {
            let generator_ref_1 = object.custom_iter_ref()
        }
        let mut generator_ref_2 = object.custom_iter_ref()
        generator_ref_2.res()
    }
)");
