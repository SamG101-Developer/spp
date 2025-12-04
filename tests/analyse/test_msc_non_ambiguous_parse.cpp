#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestNonAmbiguousParse,
    test_valid_calling_with_cmp_integer, R"(
    fun v[cmp n: std::number::U64]() -> std::boolean::Bool {
        ret true
    }

    fun g() -> std::void::Void {
        let v = std::vector::Vec[std::function::FunMov[(), std::string::Str]]()
        let mut x = v[1_u64]()
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestNonAmbiguousParse,
    test_valid_indexing_vector_of_callables, R"(
    fun g() -> std::void::Void {
        let v = std::vector::Vec[std::function::FunMov[(), std::string::Str]]()
        let mut x = (v[1_u64])()
        x = "hello"
    }
)");
