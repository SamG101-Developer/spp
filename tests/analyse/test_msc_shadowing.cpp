#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestShadowing,
    test_shadow_create_inner_doesnt_use_outer,
    SppUninitializedMemoryUseError, R"(
    use std::void::Void
    use std::boolean::Bool

    fun f() -> Void {
        let x: Bool
        loop true {
            let x = false
        }
        let y = x
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestShadowing,
    test_shadow_use_inner_uses_outer, R"(
    use std::void::Void
    use std::boolean::Bool

    fun f() -> Void {
        let x: Bool
        loop true {
            x = false
        }
        let y = x
    }
)");;
