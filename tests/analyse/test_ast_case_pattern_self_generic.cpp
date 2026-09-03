#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternSelfGeneric,
  test_valid_self_as_a_pattern_generic_argument, R"(
    cls W[T] { !public v: T }

    sup [T: std::copy::Copy] W[T] ext std::copy::Copy { }

    sup [T: std::copy::Copy] W[T] {
        fun maybe(self) -> std::option::Opt[Self] {
            ret std::option::Some(val=self)
        }

        fun present(self) -> Bool {
            case self.maybe() is std::option::Some[Self](val) { ret true }
            ret false
        }
    }

    fun f() -> Void {
        let w = W[S32](v=1)
        let a = w.present()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternSelfGeneric,
  test_valid_self_pattern_instantiated_twice, R"(
    cls W[T] { !public v: T }

    sup [T: std::copy::Copy] W[T] ext std::copy::Copy { }

    sup [T: std::copy::Copy] W[T] {
        fun maybe(self) -> std::option::Opt[Self] {
            ret std::option::Some(val=self)
        }

        fun present(self) -> Bool {
            case self.maybe() is std::option::Some[Self](val) { ret true }
            ret false
        }
    }

    fun f() -> Void {
        let a = W[S32](v=1).present()
        let b = W[Bool](v=true).present()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternSelfGeneric,
  test_valid_concrete_pattern_generic_argument, R"(
    cls W[T] { !public v: T }

    sup [T: std::copy::Copy] W[T] {
        fun present(o: std::option::Opt[S32]) -> Bool {
            case o is std::option::Some[S32](val) { ret true }
            ret false
        }
    }

    fun f() -> Void {
        let a = W[S32]::present(std::option::Some(val=1))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternSelfGeneric,
  test_valid_self_pattern_in_a_case_of_block, R"(
    cls W[T] { !public v: T }

    sup [T: std::copy::Copy] W[T] ext std::copy::Copy { }

    sup [T: std::copy::Copy] W[T] {
        fun maybe(self) -> std::option::Opt[Self] {
            ret std::option::Some(val=self)
        }

        fun present(self) -> Bool {
            ret case self.maybe() of {
                is std::option::Some[Self](val) { true }
                else { false }
            }
        }
    }

    fun f() -> Void {
        let a = W[S32](v=1).present()
    }
)");
