#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestNonAmbiguousParse,
    test_valid_calling_with_cmp_integer, R"(
    fun v[cmp n: U64]() -> Bool {
        ret true
    }

    fun g() -> Void {
        let v = Vec[FunMov[(), Str]]()
        let mut x = v[1_u64]()
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestNonAmbiguousParse,
    test_valid_indexing_vector_of_callables, R"(
    fun g() -> Void {
        let v = Vec[FunMov[(), StrView]]()
        let mut x = (v[1_u64])()
        x = "hello"
    }
)");
