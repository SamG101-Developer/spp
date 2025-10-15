#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_partial_move_from_yielded_borrow_via_variable,
    SppMoveFromPinnedMemoryError, R"(
    cls A {
        a: std::string::Str
    }

    cor g() -> std::generator::Gen[&A] { }

    fun f() -> std::void::Void {
        let mut generator = g()
        let a = generator.res()
        let b = iter a of {
            value { value.a }
            !! { "nothing" }
        }
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_partial_move_from_yielded_borrow_directly,
    SppMoveFromPinnedMemoryError, R"(
    cls A {
        a: std::string::Str
    }

    cor g() -> std::generator::Gen[&A] { }

    fun f() -> std::void::Void {
        let mut generator = g()
        let b = iter generator.res() of {
            value { value.a }
            !! { "nothing" }
        }
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_mut_borrow_after_conflicting_ref_borrow_created_simple,
    SppMemoryOverlapUsageError, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str] { }

    fun h(a: &mut std::string::Str) -> std::void::Void { }

    fun f() -> std::void::Void {
        let mut x = "hello world"
        let mut coroutine = g(&x)
        h(&mut x)
        coroutine.res()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_ref_borrow_after_conflicting_mut_borrow_created_simple,
    SppMemoryOverlapUsageError, R"(
    cor g(a: &mut std::string::Str) -> std::generator::Gen[std::string::Str] { }

    fun h(a: &std::string::Str) -> std::void::Void { }

    fun f() -> std::void::Void {
        let mut x = "hello world"
        let mut coroutine = g(&mut x)
        h(&x)
        coroutine.res()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_mut_borrow_after_conflicting_mut_borrow_created_simple,
    SppMemoryOverlapUsageError, R"(
    cor g(a: &mut std::string::Str) -> std::generator::Gen[std::string::Str] { }

    fun h(a: &mut std::string::Str) -> std::void::Void { }

    fun f() -> std::void::Void {
        let mut x = "hello world"
        let mut coroutine = g(&mut x)
        h(&mut x)
        coroutine.res()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_ref_borrow_after_conflicting_ref_borrow_created_simple, R"(
    cor g(a: &std::string::Str) -> std::generator::Gen[std::string::Str] { }

    fun h(a: &std::string::Str) -> std::void::Void { }

    fun f() -> std::void::Void {
        let x = "hello world"
        let mut coroutine = g(&x)
        h(&x)
        coroutine.res()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_define_conflicting_mut_borrow_after_mut_borrow_created, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let object = MyType()
        let generator_ref_1 = object.custom_iter_ref()
        let generator_ref_2 = object.custom_iter_ref()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_mut_borrow_after_conflicting_ref_borrow_created,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_mut = object.custom_iter_mut()
        let generator_ref = object.custom_iter_ref()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_ref_borrow_after_conflicting_mut_borrow_created,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let generator_ref = object.custom_iter_ref()
        let mut generator_mut = object.custom_iter_mut()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_mut_borrow_after_conflicting_mut_borrow_created,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_mut_1 = object.custom_iter_mut()
        let mut generator_mut_2 = object.custom_iter_mut()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_ref_borrow_after_conflicting_ref_borrow_created,
    R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str] { }
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_ref_1 = object.custom_iter_ref()
        let generator_ref_2 = object.custom_iter_ref()
        generator_ref_1.res()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_mut_borrow_after_conflicting_ref_borrow_created_with_scoping,
    SppMemoryOverlapUsageError, R"(
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
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_ref_borrow_after_conflicting_mut_borrow_created_with_scoping,
    SppMemoryOverlapUsageError, R"(
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
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_mut_borrow_after_conflicting_mut_borrow_created_with_scoping,
    SppMemoryOverlapUsageError, R"(
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
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_ref_borrow_after_conflicting_ref_borrow_created_with_scoping,
    R"(
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
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstYieldedBorrow,
    test_invalid_use_mut_borrow_after_conflicting_mut_borrow_created_for_resume,
    SppMemoryOverlapUsageError, R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_mut(&mut self) -> std::generator::Gen[&mut std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_mut = object.custom_iter_mut()
        let x = generator_mut.res()
        let y = generator_mut.res()
        let z = iter x of {
            val { val.to_ascii_uppercase() }
            !! { "" }
        }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_ref_borrow_after_conflicting_ref_borrow_created_for_resume,
    R"(
    cls MyType { }
    sup MyType {
        cor custom_iter_ref(&self) -> std::generator::Gen[&std::string::Str, std::void::Void] { }
    }

    fun test() -> std::void::Void {
        let mut object = MyType()
        let mut generator_ref = object.custom_iter_ref()
        let x = generator_ref.res()
        let y = generator_ref.res()
        let z = iter x of {
            val { val.to_ascii_uppercase() }
            !! { "" }
        }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_mut_borrow_after_conflicting_ref_borrow_created_with_scoping,
    R"(
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
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_ref_borrow_after_conflicting_mut_borrow_created_with_scoping,
    R"(
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
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_mut_borrow_after_conflicting_mut_borrow_created_with_scoping,
    R"(
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
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstYieldedBorrow,
    test_valid_use_ref_borrow_after_conflicting_ref_borrow_created_with_scoping_2,
    R"(
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
)")
