#include "test_macros.hpp"

// A non-Copy temporary is only held to linearity when it is moved somewhere. Borrowed, read from, compared or matched
// on, it is simply dropped on the floor, and there are no implicit destructors to clean it up. Each case below has a
// named counterpart that is already caught (see test_lin_scope_exit.cpp).

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_attribute_moved_off_a_temporary,
  SppLinearValueNotConsumedError, R"(
    cls TmpPairA {
        !public a: Str
        !public b: Str
    }

    fun mk() -> TmpPairA { ret TmpPairA(a=Str::from("a"), b=Str::from("b")) }

    fun f() -> Void {
        let a = mk().a
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_copyable_attribute_read_off_a_temporary,
  SppLinearValueNotConsumedError, R"(
    cls TmpPairB {
        !public a: S32
        !public b: Str
    }

    fun mk() -> TmpPairB { ret TmpPairB(a=1, b=Str::from("b")) }

    fun f() -> S32 {
        ret mk().a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_borrowing_method_called_on_a_temporary,
  SppLinearValueNotConsumedError, R"(
    cls TmpPeek { !public b: Str }

    sup TmpPeek {
        !public fun peek(&self) -> S32 { ret 1 }
    }

    fun mk() -> TmpPeek { ret TmpPeek(b=Str::from("b")) }

    fun f() -> S32 {
        ret mk().peek()
    }
)");

// A method's result is a new value even though "PlaceOf" derives it from the receiver's place ("v.dup()" is "v", one
// opaque step in, for the borrows a call hands back); std's "rfind_if" leaked through "self.to_reverse().chars()".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_borrowing_method_called_on_a_method_result,
  SppLinearValueNotConsumedError, R"(
    cls TmpDup { !public b: Str }

    sup TmpDup {
        !public fun dup(&self) -> TmpDup { ret TmpDup(b=Str::from("b")) }
        !public fun peek(&self) -> S32 { ret 1 }
    }

    fun f() -> S32 {
        let v = TmpDup(b=Str::from("b"))
        let n = v.dup().peek()
        std::mem::ops::drop(v)
        ret n
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_method_result_borrowed_as_an_argument,
  SppLinearValueNotConsumedError, R"(
    cls TmpArg { !public b: Str }

    sup TmpArg {
        !public fun dup(&self) -> TmpArg { ret TmpArg(b=Str::from("b")) }
    }

    fun look(x: &TmpArg) -> S32 { ret 1 }

    fun f() -> S32 {
        let v = TmpArg(b=Str::from("b"))
        let n = look(&v.dup())
        std::mem::ops::drop(v)
        ret n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestLinearTemporaries,
  test_valid_borrow_handed_back_by_a_call_is_no_temporary, R"(
    fun f() -> USize {
        let mut v = std::vector::Vec[Str]::new()
        v.append(Str::from("a"))
        let n = v[0_uz].len()
        std::mem::ops::drop(v)
        ret n
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_element_taken_off_a_temporary_tuple,
  SppLinearValueNotConsumedError, R"(
    cls TmpElem { }

    fun eat(l: TmpElem) -> Void { let TmpElem() = l }

    fun f() -> Void {
        let x = (TmpElem(), TmpElem()).0
        eat(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_temporary_passed_by_borrow,
  SppLinearValueNotConsumedError, R"(
    cls TmpBorrowed { }

    fun peek(l: &TmpBorrowed) -> Void { }

    fun f() -> Void {
        peek(&TmpBorrowed())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_temporary_case_subject,
  SppLinearValueNotConsumedError, R"(
    cls TmpSubject { }

    fun mk() -> TmpSubject or S32 { ret TmpSubject() }

    fun f() -> Void {
        case mk() of {
            is S32() { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_temporaries_compared,
  SppLinearValueNotConsumedError, R"(
    fun f() -> Bool {
        ret Str::from("a") == Str::from("b")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestLinearTemporaries,
  test_valid_temporary_consumed_by_a_chain, R"(
    cls TmpChain { }

    sup TmpChain {
        !public fun next(self) -> TmpChain { ret self }
        !public fun eat(self) -> Void { let TmpChain() = self }
    }

    fun f() -> Void {
        TmpChain().next().next().eat()
    }
)");

// A branch that keeps the temporary and never finishes still drops it on the edge that leaves the branch.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_temporary_case_subject_kept_by_a_branch_that_returns,
  SppLinearValueNotConsumedError, R"(
    cls TmpSubject { }

    fun mk() -> TmpSubject or S32 { ret TmpSubject() }

    fun f() -> Void {
        case mk() of {
            is S32() { }
            else { ret }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_temporary_case_subject_kept_by_a_branch_that_exits_a_loop,
  SppLinearValueNotConsumedError, R"(
    cls TmpSubject { }

    fun mk() -> TmpSubject or S32 { ret TmpSubject() }

    fun f() -> Void {
        loop true {
            case mk() of {
                is S32() { }
                else { exit }
            }
        }
    }
)");

// One that never leaves at all (an abort) has no edge to drop it on; nor does an "exit" from a loop inside the branch.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestLinearTemporaries,
  test_valid_temporary_case_subject_kept_by_a_branch_that_aborts, R"(
    cls TmpSubject { }

    fun mk() -> TmpSubject or S32 { ret TmpSubject() }

    fun f() -> Void {
        case mk() of {
            is S32() { }
            else {
                let mut i = 0
                loop i < 3 {
                    i = i + 1
                    exit
                }
                std::abort::abort()
            }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestLinearTemporaries,
  test_invalid_temporary_case_subject_kept_by_falling_through,
  SppLinearValueNotConsumedError, R"(
    cls TmpSubject { }

    fun mk() -> TmpSubject or S32 { ret TmpSubject() }

    fun f() -> Void {
        case mk() of {
            is S32() { }
        }
    }
)");
