#include "../test_macros.hpp"

// The base class. A literal names nothing, so it is carried through untouched even while its neighbours are being
// rewritten - and it still has to survive being cloned and re-analysed at the call site, which is new work the
// substitution does for every comp default.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_literal_default_is_carried_through, R"(
    fun f[T](t: T, n: S32 = 5) -> S32 {
        ret n
    }

    fun g() -> Void {
        let x = f(true)
    }
)");

// "TypeAst", reached through "PostfixExpressionAst". A type only ever sits in expression position underneath a postfix
// operator, so the two nodes are pinned together rather than by two near-identical tests. "Wrap[T]" has to become
// "Wrap[S32]" before the caller can make sense of it.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_type_named_in_a_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](w: Wrap[T] = Wrap[T]::new()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

// "IdentifierAst". A comp parameter's name is written as a type in the parameter list and read as an identifier in an
// expression, so a default naming a sibling parameter is only substitutable once those two spellings are brought
// together. Nothing else in the compiler rewrites an identifier against a binding.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_identifier_naming_a_sibling_comp_parameter, R"(
    fun f[cmp a: U8, cmp b: U8 = a]() -> U8 {
        ret b
    }

    fun g() -> Void {
        let x = f[a=5_u8]()
    }
)");

// "ParenthesisedExpressionAst". The parentheses hold nothing themselves, so the whole of the work is recursing through
// them - and not recursing leaves the type inside written in the callee's terms.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_parenthesised_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](w: Wrap[T] = (Wrap[T]::new())) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

// "TupleLiteralAst". Each element is an expression in its own right, and only one of them names anything - which is
// what catches a walk that stops at the first element or rewrites the tuple as a whole.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_tuple_literal_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](p: (S32, Wrap[T]) = (1, Wrap[T]::new())) -> Void {
        let (n, w) = p
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

// "ArrayLiteralExplicitElementsAst". As above, but the elements are all of one type, so every one of them has to be
// rewritten rather than just the first.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_explicit_element_array_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](a: Arr[Wrap[T], 2_uz] = [Wrap[T]::new(), Wrap[T]::new()]) -> Void {
        let [p, q] = a
        std::mem::ops::drop(p)
        std::mem::ops::drop(q)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

// "ArrayLiteralRepeatedElementAst". A repeated element must superimpose "Copy", so the generic is carried by a sized
// integer rather than by a class of this file's own. The count is walked by the same override, and takes the identifier
// path above when it names a comp parameter.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_repeated_element_array_default, R"(
    use std::num::sized_integer_unsigned::SizedIntegerUnsigned

    fun f[cmp w: U32](a: Arr[SizedIntegerUnsigned[w], 2_uz] = [SizedIntegerUnsigned[w]::from(0); 2_uz]) -> Void {
        let [p, q] = a
    }

    fun g() -> Void {
        f[32_u32]()
    }
)");

// "ObjectInitializerAst". It names its type outright rather than through a postfix operator, which is the one shape a
// walk that only handles "A::b()" misses.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_object_initializer_default, R"(
    cls Holder[T] {
        !public n: S32
    }

    fun f[T](h: Holder[T] = Holder[T](n=1)) -> Void {
        std::mem::ops::drop(h)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

// "BinaryExpressionAst". Both operands are walked, and the node is rebuilt rather than cloned: a clone carries over the
// function the expression was already mapped onto, and that mapping was made for the operands as they were *written*,
// so keeping it would leave the substitution with no effect and no diagnostic.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_binary_expression_default, R"(
    fun f[cmp n: S32](x: S32 = n + 1) -> S32 {
        ret x
    }

    fun g() -> Void {
        let y = f[4]()
    }
)");

// "PostfixExpressionOperatorFunctionCallAst", generic-argument half. The arguments written at a call *inside* a default
// are in the callee's terms too, so "make[T]()" has to become "make[S32]()".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_generic_arguments_of_a_call_in_a_default, R"(
    cls Wrap[T] { }

    fun make[T]() -> Wrap[T] {
        ret Wrap[T]()
    }

    fun f[T](w: Wrap[T] = make[T]()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

// "PostfixExpressionOperatorFunctionCallAst", function-argument half. The same walk, one field over: the runtime
// arguments of that call are expressions in their own right.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_call_arguments_of_a_call_in_a_default, R"(
    fun twice(x: S32) -> S32 {
        ret x * 2
    }

    fun f[cmp n: S32](x: S32 = twice(n)) -> S32 {
        ret x
    }

    fun g() -> Void {
        let y = f[4]()
    }
)");

// "UnaryExpressionAst". "async" is the only unary operator, and it is a keyword - so the whole of the work is the
// operand, which is a call carrying the generic.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_async_default, R"(
    cls Wrap[T] { }

    fun make[T]() -> Wrap[T] {
        ret Wrap[T]()
    }

    fun f[T](w: Fut[Wrap[T]] = async make[T]()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

// Two instantiations of one default, so that the substituted clone really is per-instantiation. A walk that rewrote the
// prototype's own default in place rather than answering with a new tree would pass every test above and fail this one:
// the second call would see the first call's arguments already baked in.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_one_default_substituted_two_ways, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](w: Wrap[T] = Wrap[T]::new()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
        f[Bool]()
    }
)");

// Todo: "PostfixExpressionOperatorIndexAst", "PostfixExpressionOperatorSliceAst" and
//  "PostfixExpressionOperatorKeywordResAst" override the walk but have no test, because neither consumer can reach
//  them. Both materialise an expression that has no receiver in scope - a default is written where no local exists -
//  and each of those three operators needs one. Indexing or slicing a temporary hands back a borrow of it, and ".res"
//  needs a generator to resume. The overrides are there so that the walk is complete rather than because a default can
//  use them today; add tests here if a third consumer of "SubstituteGenericsExpr" ever appears.
