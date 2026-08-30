#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestShadowing,
  test_shadow_create_inner_doesnt_use_outer,
  SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x: Bool
        loop true {
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
