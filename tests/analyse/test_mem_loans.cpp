#include "../test_macros.hpp"

// Every borrow held past the expression that made it is one loan, held by a symbol: a coroutine or future handle, a
// closure, the binding a yielded or indexed borrow is given. These pin the rules the loans share.

// A binding given a new value no longer holds what its old one borrowed: the second index borrow does not overlap
// the first.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemLoans,
  test_valid_reassigned_borrow_releases_its_old_loan, R"(
    fun f() -> Void {
        let mut v = std::vector::Vec[S32]::new()
        {
            let mut a = v[mut 0_uz]
            a = v[mut 1_uz]
        }
        std::mem::ops::drop(v)
    }
)");

// A handle holding loans cannot be moved, so it can never be consumed; writing over it would throw it away, which no
// assignment may do.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_reassigning_a_handle_that_holds_borrows,
  SppLinearValueNotConsumedError, R"(
    cor c(a: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let x = Str::from("1")
        let y = Str::from("2")
        {
            let mut h = c(&x)
            h = c(&y)
        }
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
    }
)");

// A borrow held on only one of the paths that met is released with its holder like any other, and leaves nothing
// behind to disagree about.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemLoans,
  test_valid_borrow_held_on_one_path_released_with_its_holder, R"(
    cor c(a: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f(b: Bool) -> Void {
        let x = Str::from("1")
        let y = Str::from("2")
        {
            let h: Gen[&StrView]
            case b {
                h = c(&x)
            }
            else {
                h = c(&y)
            }
            std::mem::ops::drop(h)
        }
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
    }
)");

// A handle moved into another name carries its borrows with it.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_handle_moved_by_assignment_keeps_its_borrows,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor c(a: &Str) -> Gen[&StrView] {
        gen "0"
    }

    fun f() -> Void {
        let x = Str::from("1")
        let h = c(&x)
        let h2: Gen[&StrView]
        h2 = h
        std::mem::ops::drop(x)
        let r = h2.res()
    }
)");

// A "&mut" a generator yielded points into the generator, which cannot be consumed while it is held.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_generator_consumed_while_its_yielded_mut_borrow_is_held,
  SppMovingEscapingBorrowedMemoryError, R"(
    cor c() -> Gen[&mut S32] {
        let mut n = 0_s32
        gen &mut n
    }

    fun f() -> Void {
        let mut g = c()
        let a = g.res()
        std::mem::ops::drop(g)
        let b = a
    }
)");

// Resuming the generator again ends what it yielded before, as before loans.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_yielded_mut_borrow_used_after_the_next_resume,
  SppUninitializedMemoryUseError, R"(
    cor c() -> Gen[&mut S32] {
        let mut n = 0_s32
        gen &mut n
        gen &mut n
    }

    fun f() -> Void {
        let mut g = c()
        let a = g.res()
        let b = g.res()
        let d = a
    }
)");

// An index or slice borrow given a name is held by it: a second "&mut" of the same value overlaps it (M7.2), when the
// first is used again (M9.2).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_index_borrow_held_by_its_binding,
  SppMemoryOverlapUsageError, R"(
    fun f() -> Void {
        let mut v = std::vector::Vec[S32]::new()
        let a = v[mut 0_uz]
        let b = v[mut 1_uz]
        let z = a@
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_slice_borrow_held_by_its_binding,
  SppMemoryOverlapUsageError, R"(
    fun f() -> Void {
        let mut v = std::vector::Vec[S32]::new()
        let a = v[mut 0_uz to 2_uz]
        let b = v[mut 1_uz]
        let z = a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_mut_index_borrow_while_a_ref_one_is_held,
  SppMemoryOverlapUsageError, R"(
    fun f() -> Void {
        let mut v = std::vector::Vec[S32]::new()
        let a = v[0_uz]
        let c = v[mut 1_uz]
        let z = a@
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemLoans,
  test_valid_index_borrow_released_with_its_binding, R"(
    fun f() -> Void {
        let mut v = std::vector::Vec[S32]::new()
        {
            let a = v[mut 0_uz]
        }
        {
            let b = v[mut 1_uz]
        }
        std::mem::ops::drop(v)
    }
)");

// M9.2: a loan lasts to its holder's last use. A conflict with it is an error only if the holder is used again.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemLoans,
  test_valid_index_borrows_after_the_last_use_of_the_first, R"(
    fun f() -> Void {
        let mut v = std::vector::Vec[S32]::from(&[1, 2])
        let a = v[mut 0_uz]
        a@ = 5
        let b = v[mut 1_uz]
        b@ = 6
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_index_borrow_used_after_a_second_one,
  SppMemoryOverlapUsageError, R"(
    fun f() -> Void {
        let mut v = std::vector::Vec[S32]::from(&[1, 2])
        let a = v[mut 0_uz]
        let b = v[mut 1_uz]
        a@ = 5
        std::mem::ops::drop(v)
    }
)");

// A use earlier in a loop's body is a use after the conflict on the next iteration.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_index_borrow_used_again_round_a_loop,
  SppMemoryOverlapUsageError, R"(
    fun f(b: Bool) -> Void {
        let mut v = std::vector::Vec[S32]::from(&[1, 2])
        {
            let a = v[mut 0_uz]
            loop b {
                a@ = 1
                let c = v[mut 1_uz]
            }
        }
        std::mem::ops::drop(v)
    }
)");

// The shape of "Opt::take_if": the subject is moved after the pattern's borrow of it was last used.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemLoans,
  test_valid_subject_moved_after_the_last_use_of_a_pattern_borrow, R"(
    fun keep_if(o: Opt[S32]) -> Opt[S32] {
        case o of {
            is Some[S32](&val) {
                case val@ > 0 { ret o }
            }
        }
        ret o
    }
)");

// A generator used after its borrowed value was borrowed again.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_generator_resumed_after_a_conflicting_borrow,
  SppMemoryOverlapUsageError, R"(
    cor co(a: &mut S32) -> Gen[S32] {
        gen 1
    }

    fun write(a: &mut S32) -> Void { }

    fun f() -> Void {
        let mut x = 1_s32
        let mut g = co(&mut x)
        write(&mut x)
        let r = g.res()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestMemLoans,
  test_valid_generator_not_resumed_after_a_conflicting_borrow, R"(
    cor co(a: &mut S32) -> Gen[S32] {
        gen 1
    }

    fun write(a: &mut S32) -> Void { }

    fun f() -> Void {
        let mut x = 1_s32
        let mut g = co(&mut x)
        let r = g.res()
        write(&mut x)
        std::mem::ops::drop(g)
    }
)");

// A future may be running while it is held, so its borrow is live to the end of its scope, used again or not.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_borrow_while_a_future_holds_one_whatever_follows,
  SppMemoryOverlapUsageError, R"(
    fun m(a: &mut S32) -> S32 { ret 1 }
    fun read(y: &S32) -> Void { }

    fun f() -> Void {
        let mut x = 1_s32
        let fut = async m(&mut x)
        read(&x)
    }
)");

// A holder given to a call hands it the ways to what it borrows: two ways to one place in one call, at once.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestMemLoans,
  test_invalid_closure_borrowed_beside_what_it_captures,
  SppMemoryOverlapUsageError, R"(
    fun take[F](c: &F, y: &mut S32) -> Void { }

    fun f() -> Void {
        let mut x = 1_s32
        let mut c = (caps &mut x) { x@ = 2_s32 }
        take(&c, &mut x)
    }
)");
