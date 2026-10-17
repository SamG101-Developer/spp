#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestShadowing,
  test_shadow_create_inner_doesnt_use_outer,
  SppUninitializedMemoryUseError, R"(
    fun f(b: Bool) -> Void {
        let x: Bool
        loop b {
            let x = false
        }
        let y = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_use_inner_uses_outer, R"(
    fun f() -> Void {
        let x: Bool
        loop true {
            x = false
            exit
        }
        let y = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_same_scope_different_type, R"(
    fun f() -> Void {
        let x = 123
        let x = true
        let mut y = x
        y = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_same_scope_different_type_each_keeps_its_own_storage, R"(
    fun f() -> Void {
        let v = Vec[S32]::from(&[1, 2, 3])
        std::mem::ops::drop(v)
        let v = Vec[Bool]::from(&[true])
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_same_scope_value_reads_the_binding_before, R"(
    fun f() -> Bool {
        let x = 5
        let x = x == 5
        ret x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_same_scope_chain_of_three, R"(
    fun f() -> S32 {
        let a = 1
        let a = a + 1
        let a = a + 1
        ret a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_same_scope_different_mutability, R"(
    fun f() -> Void {
        let x = 1
        let mut x = 2
        x = 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_inner_scope_does_not_change_outer_type, R"(
    fun f() -> Void {
        let mut x: Bool = true
        {
            let x: S32 = 5
        }
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_function_parameter, R"(
    fun f(x: Bool) -> Void {
        let x = 5
        let mut y = x
        y = 6
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_moved_variable, R"(
    fun f() -> (Str, Str) {
        let x = Str::from("a")
        let y = x
        let x = Str::from("b")
        let z = x
        ret (y, z)
    }
)");

// A method's mock constant does not shadow the module-level "drop" the prelude brings in. "drop" here is both that
// free function and a method on "T", and the unqualified call in "consume" means the free function - the method is
// only reachable through a receiver. Nothing about this depends on "Drop"; the method is an ordinary one that happens
// to share the name. The import is not written out because the prelude already provides it, and repeating it is a
// duplicate identifier.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_method_does_not_hide_module_import, R"(
    cls T { }

    sup T {
        fun drop(self) -> Void {
            let T() = self
        }

        fun consume(&self, s: std::string::Str) -> Void {
            drop(s)
        }
    }
)");

// A name means the binding written before the point reading it, in every stage, not only the one that made the
// symbols: each "let" declares its binding where it is written (see "Scope::HideLetBindings"). A destructure that
// shadows its own inputs reads them through its hidden temporary, which a per-"let" patch never covered - the value
// "(b, a)" read the new "a" and "b" from stage 8 on.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_same_scope_destructure_swaps_its_own_inputs, R"(
    fun f() -> Void {
        let (a, b) = (Str::from("1"), Str::from("2"))
        let (a, b) = (b, a)
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_shadow_same_scope_destructure_swaps_its_own_inputs_copyable, R"(
    fun f() -> (S32, S32) {
        let (a, b) = (1, 2)
        let (a, b) = (b, a)
        ret (a, b)
    }
)");

// An "is" narrows its subject from where it is written onwards. Stage 7 left the narrowed symbol filed for the whole
// scope, so a "case" over the subject before the "is" was checked and generated against it already narrowed to
// "Some", not the "Opt" it is.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestShadowing,
  test_is_narrowing_does_not_reach_back_before_the_is, R"(
    fun f(o: Opt[Str]) -> S32 {
        let n = case o of {
            is Some[Str](&val) { 1 }
            else { 0 }
        }
        let b = o is Some[Str](&val)
        std::mem::ops::drop(o)
        ret n
    }
)");
