#include "../test_macros.hpp"

// Superimposition shapes that are legal but rarely written: a blanket block over every type, the same with a
// constraint, blanket extensions, and blocks whose pattern is a variadic pack. Each one exercises pattern matching and
// generic binding at the edge of what "SupPatternApplies" has to answer, and several have broken before - a blanket
// "sup [T] T" is appended to every scope's candidate list, including the dummy scope of a generic parameter.
//
// A bare "sup [T] T { }" is covered by "AstSupPrototypeFunctionsAst.test_valid_sup_prototype_functions_onto_generic_type"
// and "TestSpecialization.test_blanket_specialization", and "sup [X: A] C ext B[X]" by
// "TestAstGenericConstraints.test_valid_ext_constraint", so they are not repeated here.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_blanket_sup_with_constraint, R"(
    sup [T: Copy] T {
        !public fun copy_only(&self) -> S32 { ret 1 }
    }

    fun f() -> Void {
        let x = 1.copy_only()
    }
)");

// The constraint has to gate the blanket, not decorate it: a type that does not satisfy "Copy" never gets the method.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNicheSuperimpositions,
  test_blanket_sup_with_constraint_excludes_non_conforming,
  SppIdentifierUnknownError, R"(
    cls NotCopy { }

    sup [T: Copy] T {
        !public fun copy_only(&self) -> S32 { ret 1 }
    }

    fun f() -> Void {
        let x = NotCopy()
        let y = x.copy_only()
    }
)");

// A blanket extension confers the extended type's methods on everything, so the method is reached through the "ext".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_blanket_sup_extension, R"(
    cls Other { }

    sup Other {
        !public fun from_other(&self) -> S32 { ret 7 }
    }

    sup [T] T ext Other { }

    fun f() -> Void {
        let x = 1.from_other()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_blanket_sup_extension_with_constraint, R"(
    cls Other { }

    sup Other {
        !public fun from_other(&self) -> S32 { ret 7 }
    }

    sup [T: Copy] T ext Other { }

    fun f() -> Void {
        let x = 1.from_other()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNicheSuperimpositions,
  test_blanket_sup_extension_with_constraint_excludes_non_conforming,
  SppIdentifierUnknownError, R"(
    cls Other { }

    sup Other {
        !public fun from_other(&self) -> S32 { ret 7 }
    }

    cls NotCopy { }

    sup [T: Copy] T ext Other { }

    fun f() -> Void {
        let x = NotCopy()
        let y = x.from_other()
    }
)");

// The extended type names the very parameter the blanket binds, so every type is extended by a different instantiation.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_blanket_sup_extension_generic_of_itself, R"(
    cls Other[T] { }

    sup [T] T ext Other[T] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_blanket_sup_extension_generic_of_itself_with_constraint, R"(
    cls Other[T] { }

    sup [T: Copy] T ext Other[T] { }
)");

// One pack, named on both sides: the elements bound by the pattern are the ones the extension is instantiated over.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_variadic_sup_same_pack_on_both_sides, R"(
    cls Type1[..T] { }
    cls Type2[..T] { }

    sup [..T] Type2[T] {
        !public fun tagged(&self) -> S32 { ret 3 }
    }

    sup [..T] Type1[T] ext Type2[T] { }

    fun f() -> Void {
        let x = Type1[S32, Bool]()
        let y = x.tagged()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_variadic_sup_same_pack_on_both_sides_with_constraint, R"(
    cls Type1[..T] { }
    cls Type2[..T] { }

    sup [..T: Copy] Type1[T] ext Type2[T] { }
)");

// Two packs, only one of which the pattern binds: "U" is fixed by the extension rather than by the type matched.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNicheSuperimpositions,
  test_variadic_sup_two_distinct_packs, R"(
    cls Type1[..T] { }
    cls Type2[..U] { }

    sup [..U] Type2[U] {
        !public fun tagged(&self) -> S32 { ret 5 }
    }

    sup [..T, ..U] Type1[T] ext Type2[U] { }

    fun f() -> Void {
        let x = Type1[S32]()
        std::mem::ops::drop(x)
    }
)");
