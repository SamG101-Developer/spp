#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_custom_del_runs_at_scope_exit, R"(
    cls A { }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun main() -> Void {
        let a = A()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_attribute_destroyed_with_owner, R"(
    cls A { }

    cls Holder {
        inner: A
    }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun main() -> Void {
        let h = Holder(inner=A())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_moved_value_destroyed_once, R"(
    cls A { }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun main() -> Void {
        let a = A()
        let b = a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_conditionally_moved_value_uses_drop_flag, R"(
    cls A { }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun consume(x: A) -> Void { }

    fun cond() -> Bool {
        ret true
    }

    fun main() -> Void {
        let a = A()
        case cond() { consume(a) }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_loop_exit_destroys_body_locals, R"(
    cls A { }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun main() -> Void {
        let mut i = 0
        loop i < 5 {
            let a = A()
            case i == 2 { exit }
            i += 1
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_generic_body_local_destroyed, R"(
    cls A { }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun generic_body[T](t: T) -> Void {
        let inner = A()
    }

    fun main() -> Void {
        generic_body(0)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_generic_body_moved_out_not_destroyed, R"(
    cls A { }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun generic_moves[T](t: T) -> A {
        let inner = A()
        ret inner
    }

    fun main() -> Void {
        let m = generic_moves(0)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_generator_frame_destroyed_when_drained, R"(
    cor counter() -> Gen[S32] {
        gen 1
        gen 2
    }

    fun main() -> Void {
        loop x in counter() { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_generator_frame_destroyed_when_abandoned, R"(
    cor counter() -> Gen[S32] {
        gen 1
        gen 2
    }

    fun main() -> Void {
        loop x in counter() { exit }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    Destructors,
    test_valid_discarded_statement_value_destroyed, R"(
    cls A { }

    sup A ext std::ops::del::Del {
        fun del(&mut self) -> Void { }
    }

    fun make() -> A {
        ret A()
    }

    fun main() -> Void {
        make()
        let after = 1
    }
)");
