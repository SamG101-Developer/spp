#include "../test_macros.hpp"

// A variadic parameter declares one element ("..b: T") but binds the whole tuple the call collapsed its trailing
// arguments into. Codegen knew that - the pack type is what the parameter is emitted as - but the symbol in the body
// kept the element type, so naming the pack as a value inferred one element while a tuple was passed, and the call
// failed LLVM verification rather than anything earlier.

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariadicPack,
    test_valid_pack_used_as_a_value_is_the_tuple, R"(
    fun takes[T](v: T) -> Void {
        std::mem::ops::drop(v)
    }

    fun packed(a: Bool, ..b: S32) -> Void {
        takes(b)
    }

    fun f() -> Void {
        packed(true, 1, 2, 3)
    }
)");

// The pack's element count is part of the instantiation, so two call sites with different counts are different
// tuples rather than one type that happens to be reused.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariadicPack,
    test_valid_packs_of_different_lengths, R"(
    fun packed[T](a: Bool, ..b: T) -> Void {
        std::mem::ops::drop(b)
    }

    fun f() -> Void {
        packed(true, 1, 2, 3)
        packed(true, 4, 5)
    }
)");

// A pack over a variadic generic collects whatever it was given, of whatever types.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariadicPack,
    test_valid_pack_over_a_variadic_generic, R"(
    fun packed[..Ts](a: Bool, ..b: Ts) -> Void {
        std::mem::ops::drop(b)
    }

    fun f() -> Void {
        packed(true, 1, false, 2)
    }
)");

// A pack is a value like any other, so a body that never uses it has abandoned it - unless every element is copyable,
// which is what makes a pack of numbers usable without the function discarding it.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariadicPack,
    test_invalid_pack_of_non_copyable_elements_not_consumed,
    SppLinearValueNotConsumedError, R"(
    cls T { }

    fun packed(a: Bool, ..b: T) -> Void { }

    fun f() -> Void {
        packed(true, T(), T())
    }
)");
