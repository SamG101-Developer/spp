module;
#include <spp/macros.hpp>

module spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_size;
import spp.lex.lexer;
import spp.lex.tokens;
import spp.parse.parser_spp;
import spp.utils.ice;
import spp.utils.numbers;
import spp.utils.strings;
import genex;
import llvm;
import numex.big_dec;
import numex.big_int;

namespace spp::analyse::utils::comp_time_intrinsics {
  namespace {
    auto SizedNumericLiteralTypeName(TypeSymbol const *sym) -> Str {
      // The width and signedness are the sized type's own bindings of "w" and "signed", read from the instantiation as
      // its type arguments would be - not parsed out of how its name happens to be written.
      if (sym == nullptr) { return Str(); }
      const auto width_val = scopes::U64Of(sym->CompArgId("w"));
      if (not width_val.has_value()) { return Str(); }
      const auto width = std::to_string(*width_val);

      // A floating-point type has a width and nothing else; an integer one also says whether it is signed.
      const auto signed_id = sym->CompArgId("signed");
      if (signed_id == nullptr) { return "f" + width; }
      auto const *const signed_val = signed_id->AsBool();
      const auto is_signed = signed_val != nullptr and *signed_val;
      return (is_signed ? "s" : "u") + width;
    }

    auto SizedNumericLiteralTypeNameOrError(
      ScopeManager const &sm, Vec<Shared<TypeRef>> const &types, const bool is_float) -> Str {
      // Get the type suffix for this integer or float
      // type, and check that it is known.
      SPP_ASSERT(not types.empty());
      const auto name = SizedNumericLiteralTypeName(types[0]->Symbol);
      const auto known = is_float
        ? spp::utils::numbers::FloatBounds().contains(name)
        : spp::utils::numbers::IntegerBounds().contains(name);

      // Every width the language has a sized number type for has
      // bounds ("utils::numbers"); one without is a compiler bug.
      if (not known) {
        SPP_ASSERT(types[0]->Symbol != nullptr);
        const auto type = types[0]->Symbol->FqName();
        Ice(*type, *sm.CurrentScope, "no numeric bounds for this type");
      }
      return name;
    }

    /// Comp-time float arithmetic rounds as run-time arithmetic
    /// does: to nearest, ties to even.
    constexpr auto kNearest = llvm::APFloat::rmNearestTiesToEven;

    /// A float literal's value in its type's format: what its text
    /// rounds to at run time ("utils::numbers"). Read from the text
    /// rather than "BigVal", which loses the sign of a zero.
    auto ValueOf(FloatLiteralAst const &lit) -> llvm::APFloat {
      const auto is_negative = lit.TokSign != nullptr and lit.TokSign->TokenType == lex::SppTokenType::TK_SUB;
      const auto digits = spp::utils::strings::NormalizeFloatString(lit.IntVal->TokenData, lit.FracVal->TokenData);
      return spp::utils::numbers::ReadFloat((is_negative ? "-" : "") + digits, lit.Type);
    }

    /// A value in the format of "type" as a literal, its sign of
    /// zero kept. One no constant of the type can hold (an infinity
    /// or a NaN) is kept out of range, for the caller to reject
    /// (E16).
    auto FromValue(llvm::APFloat const &value, Str const &type) -> Unique<FloatLiteralAst> {
      if (const auto text = spp::utils::numbers::WriteFloat(value); text.has_value()) {
        return FloatLiteralAst::FromText(*text, type);
      }
      auto const &[lower, upper] = spp::utils::numbers::FloatBounds().at(type);
      return FloatLiteralAst::FromBigVal(value.isNegative() ? lower + lower : upper + upper, type);
    }

    auto AssignInPlace(
      IntegerLiteralAst &lhs, Unique<IntegerLiteralAst> &&result) -> void {
      lhs.TokSign = std::move(result->TokSign);
      lhs.Val = std::move(result->Val);
      lhs.Type = std::move(result->Type);
    }

    auto AssignInPlace(
      FloatLiteralAst &lhs, Unique<FloatLiteralAst> &&result) -> void {
      lhs.TokSign = std::move(result->TokSign);
      lhs.IntVal = std::move(result->IntVal);
      lhs.FracVal = std::move(result->FracVal);
      lhs.Type = std::move(result->Type);
    }

    /// The argument setting @p name in an object initializer,
    /// if there is one. An autofill argument ("..old") sets no
    /// attribute of its own, whatever its value is called.
    auto FindNamedArg(
      ObjectInitializerAst const &init,
      IdentifierAst const &name)
      -> ObjectInitializerArgumentAst* {
      for (auto *const arg : init.ArgGroup->GetAllArgs()) {
        const auto shorthand = arg->To<ObjectInitializerArgumentShorthandAst>();
        if (shorthand != nullptr and shorthand->TokEllipsis != nullptr) { continue; }
        if (arg->Name != nullptr and *arg->Name == name) { return arg; }
      }
      return nullptr;
    }
  }
}

auto spp::analyse::utils::comp_time_intrinsics::SetCompTimeAttrValue(
  ObjectInitializerAst const *object, Ast const *attribute,
  Unique<ExpressionAst> &&value, ScopeManager const *sm) -> void {
  // Firstly, we need to split the "attribute" as it may be
  // a dotted path.
  auto attr_path = Vec<Shared<IdentifierAst>>();
  while (true) {
    // Ensure we are looking at a postfix expression.
    const auto postfix = attribute->To<PostfixExpressionAst>();
    if (postfix == nullptr) { break; }

    // Ensure the operator is a runtime member access.
    const auto member_access = postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
    if (member_access == nullptr) { break; }

    // Push the member name to the path and continue down the
    // lhs.
    attr_path.push_back(member_access->Name);
    attribute = postfix->Lhs.get();
  }

  // Reverse the attribute path to get the correct order. The
  // root variable is not in it: that is "object".
  attr_path |= genex::actions::reverse;

  // Walk from the object down to the attribute being set. An
  // attribute already holding an object initializer is walked
  // into; one missing, or holding anything else, is given a
  // new initializer of its type, seeded from its old value
  // ("..old") so the rest of it is kept. The last attribute's
  // value is replaced, or added if it is missing.
  auto const *current = object;
  auto current_sym = sm->CurrentScope->FindTypeSymbol(object->Type.get());
  for (auto i = 0uz; i < attr_path.size(); ++i) {
    auto const &attr_name = attr_path[i];
    auto *const arg = FindNamedArg(*current, *attr_name);
    if (i + 1 == attr_path.size()) {
      if (arg != nullptr) { arg->Val = std::move(value); }
      else {
        current->ArgGroup->Args.emplace_back(MakeUnique<ObjectInitializerArgumentKeywordAst>(
          AstCloneShared(attr_name), nullptr, std::move(value)));
      }
      return;
    }

    const auto attr_type = current_sym->LinkedScope->FindVarSymbol(attr_name.get())->Type;
    current_sym = current_sym->LinkedScope->FindTypeSymbol(attr_type.get());
    if (const auto inner = arg != nullptr ? arg->Val->To<ObjectInitializerAst>() : nullptr; inner != nullptr) {
      current = inner;
      continue;
    }

    auto new_init = MakeUnique<ObjectInitializerAst>(current_sym->FqName(), nullptr);
    const auto new_init_ptr = new_init.get();
    if (arg != nullptr) {
      new_init->ArgGroup->Args.emplace_back(
        ObjectInitializerArgumentShorthandAst::CreateAutoFillArg(std::move(arg->Val)));
      arg->Val = std::move(new_init);
    }
    else {
      current->ArgGroup->Args.emplace_back(MakeUnique<ObjectInitializerArgumentKeywordAst>(
        AstCloneShared(attr_name), nullptr, std::move(new_init)));
    }
    current = new_init_ptr;
  }
}

auto spp::analyse::utils::comp_time_intrinsics::GetCompTimeAttrValue(
  ObjectInitializerAst const *object,
  IdentifierAst const *attribute)
  -> Unique<ExpressionAst> {
  // The argument setting the attribute, or else whatever an autofill argument ("..old") supplies for it.
  if (const auto arg = FindNamedArg(*object, *attribute); arg != nullptr) { return AstClone(arg->Val); }
  for (const auto arg : object->ArgGroup->GetAllArgs()) {
    const auto shorthand = arg->To<ObjectInitializerArgumentShorthandAst>();
    const auto source = shorthand != nullptr and shorthand->TokEllipsis != nullptr
      ? shorthand->Val->To<ObjectInitializerAst>()
      : nullptr;
    if (source == nullptr) { continue; }
    if (auto value = GetCompTimeAttrValue(source, attribute); value != nullptr) { return value; }
  }
  return nullptr;
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsAdd(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform addition on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() + rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsSub(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform subtraction on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() - rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsMul(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform multiplication on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() * rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsDiv(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform division on two integer literals, exactly.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() / rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsRem(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform remainder on two integer literals, exactly.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() % rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsSneg(
  IntegerLiteralAst const &val)
  -> Unique<IntegerLiteralAst> {
  // Perform signed negation on an integer literal.
  return IntegerLiteralAst::FromBigVal(-val.BigVal(), val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsBitShl(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise left shift on two integer literals. S++ forces U32 too (safe). In the type's width, as the
  // run-time "shl" is: the bits shifted past the top are lost.
  const auto count = rhs.CppVal<std::uint32_t>();
  const auto shifted = spp::utils::numbers::ShiftLeftLogical(lhs.BigVal(), count, lhs.Type);
  return IntegerLiteralAst::FromBigVal(shifted.value_or(lhs.BigVal() << count), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsBitShr(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise right shift on two integer literals. S++ forces U32 too (safe). Logical, in the type's width, as
  // the run-time "lshr" is: a negative value's sign bit moves down too.
  const auto count = rhs.CppVal<std::uint32_t>();
  const auto shifted = spp::utils::numbers::ShiftRightLogical(lhs.BigVal(), count, lhs.Type);
  return IntegerLiteralAst::FromBigVal(shifted.value_or(lhs.BigVal() >> count), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsBitIor(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise OR on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() | rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsBitAnd(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise AND on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() & rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsBitXor(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise XOR on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() ^ rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsBitNot(
  IntegerLiteralAst const &val)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise NOT on an integer literal, in its type's
  // width: "~" alone is unbounded, which only a signed type
  // agrees with ("~15_u32" is 2^32 - 16, not -16).
  const auto inverted = ~val.BigVal();
  const auto wrapped = spp::utils::numbers::WrapToInteger(inverted, val.Type);
  return IntegerLiteralAst::FromBigVal(wrapped.value_or(inverted), val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsBitNotAssign(
  IntegerLiteralAst &lhs)
  -> void {
  // Perform bitwise NOT assignment on an integer literal. Written out rather than generated below because it is the
  // one unary assignment; it used to hand-roll the field copy that "AssignInPlace" does, and had drifted from it.
  AssignInPlace(lhs, StdIntrinsicsBitNot(lhs));
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsEq(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform equality comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() == rhs.BigVal());
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsOeq(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered equality comparison on two float literals, on their values in
  // their type's format, as at run time: two texts that round to one value are equal.
  const auto c = ValueOf(lhs).compare(ValueOf(rhs));
  return BooleanLiteralAst::FromCppVal(c == llvm::APFloat::cmpEqual);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsNe(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform inequality comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() != rhs.BigVal());
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsOne(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered inequality comparison on two float literals, on their values in
  // their type's format, as at run time: two texts that round to one value are equal.
  const auto c = ValueOf(lhs).compare(ValueOf(rhs));
  return BooleanLiteralAst::FromCppVal(c == llvm::APFloat::cmpLessThan or c == llvm::APFloat::cmpGreaterThan);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsLt(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform less-than comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() < rhs.BigVal());
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsOlt(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered less than comparison on two float literals, on their values in
  // their type's format, as at run time: two texts that round to one value are equal.
  const auto c = ValueOf(lhs).compare(ValueOf(rhs));
  return BooleanLiteralAst::FromCppVal(c == llvm::APFloat::cmpLessThan);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsLe(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform less-than-or-equal comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() <= rhs.BigVal());
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsOle(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered less than or equal comparison on two float literals, on their values in
  // their type's format, as at run time: two texts that round to one value are equal.
  const auto c = ValueOf(lhs).compare(ValueOf(rhs));
  return BooleanLiteralAst::FromCppVal(c == llvm::APFloat::cmpLessThan or c == llvm::APFloat::cmpEqual);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsGt(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform greater-than comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() > rhs.BigVal());
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsOgt(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered greater than comparison on two float literals, on their values in
  // their type's format, as at run time: two texts that round to one value are equal.
  const auto c = ValueOf(lhs).compare(ValueOf(rhs));
  return BooleanLiteralAst::FromCppVal(c == llvm::APFloat::cmpGreaterThan);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsGe(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform greater-than-or-equal comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() >= rhs.BigVal());
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsOge(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered greater than or equal comparison on two float literals, on their values in
  // their type's format, as at run time: two texts that round to one value are equal.
  const auto c = ValueOf(lhs).compare(ValueOf(rhs));
  return BooleanLiteralAst::FromCppVal(c == llvm::APFloat::cmpGreaterThan or c == llvm::APFloat::cmpEqual);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsMaxVal(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // The highest value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(
    spp::utils::numbers::IntegerBounds().at(name).second, name);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsMinVal(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // The lowest value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(
    spp::utils::numbers::IntegerBounds().at(name).first, name);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsMax(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform maximum on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal().Max(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsMin(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform minimum on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal().Min(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsCmp(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform three-way comparison on two integer literals. The
  // result is an "S32" whatever the operands are ("-1" for an
  // unsigned one has no other type to be).
  const auto cmp = lhs.BigVal() <=> rhs.BigVal();
  const auto res = std::is_gt(cmp) - std::is_lt(cmp);
  return IntegerLiteralAst::FromBigVal(numex::BigInt(res), Str("s32"));
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFadd(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // To add two float literals: in their type's format, rounded as at run time.
  auto value = ValueOf(lhs);
  value.add(ValueOf(rhs), kNearest);
  return FromValue(value, lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFsub(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // To subtract two float literals: in their type's format, rounded as at run time.
  auto value = ValueOf(lhs);
  value.subtract(ValueOf(rhs), kNearest);
  return FromValue(value, lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFmul(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // To multiply two float literals: in their type's format, rounded as at run time.
  auto value = ValueOf(lhs);
  value.multiply(ValueOf(rhs), kNearest);
  return FromValue(value, lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFdiv(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // To divide two float literals: in their type's format, rounded as at run time.
  auto value = ValueOf(lhs);
  value.divide(ValueOf(rhs), kNearest);
  return FromValue(value, lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFrem(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // To take the remainder of two float literals ("fmod"): in their type's format, rounded as at run time.
  auto value = ValueOf(lhs);
  value.mod(ValueOf(rhs));
  return FromValue(value, lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFneg(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // To negate a float literal (keeping the sign of a zero): in its type's format, keeping the sign of a zero.
  auto value = ValueOf(val);
  value.changeSign();
  return FromValue(value, val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFabs(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // To take the absolute value of a float literal: in its type's format, keeping the sign of a zero.
  auto value = ValueOf(val);
  value.clearSign();
  return FromValue(value, val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFmaxVal(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<FloatLiteralAst> {
  // The largest finite value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, true);
  return FloatLiteralAst::FromBigVal(
    spp::utils::numbers::FloatBounds().at(name).second, name);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFminVal(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<FloatLiteralAst> {
  // The most negative finite value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, true);
  return FloatLiteralAst::FromBigVal(
    spp::utils::numbers::FloatBounds().at(name).first, name);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFmax(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // To take the maximum of two float literals ("maxnum"): in their type's format.
  return FromValue(llvm::maxnum(ValueOf(lhs), ValueOf(rhs)), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFmin(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // To take the minimum of two float literals ("minnum"): in their type's format.
  return FromValue(llvm::minnum(ValueOf(lhs), ValueOf(rhs)), lhs.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFfloor(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // To round a float literal down: in its type's format, keeping the sign of a zero.
  auto value = ValueOf(val);
  value.roundToIntegral(llvm::APFloat::rmTowardNegative);
  return FromValue(value, val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFceil(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // To round a float literal up: in its type's format, keeping the sign of a zero.
  auto value = ValueOf(val);
  value.roundToIntegral(llvm::APFloat::rmTowardPositive);
  return FromValue(value, val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFtrunc(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // To round a float literal toward zero: in its type's format, keeping the sign of a zero.
  auto value = ValueOf(val);
  value.roundToIntegral(llvm::APFloat::rmTowardZero);
  return FromValue(value, val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsicsFround(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // To round a float literal, half away from zero as "llvm.round" does: in its type's format, keeping
  // the sign of a zero.
  auto value = ValueOf(val);
  value.roundToIntegral(llvm::APFloat::rmNearestTiesToAway);
  return FromValue(value, val.Type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdNumFloatNegOne(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<FloatLiteralAst> {
  // "-1.0" as a "Self": the type the method was called on, its
  // only type argument.
  const auto type = SizedNumericLiteralTypeNameOrError(sm, types, true);
  return FloatLiteralAst::FromBigVal(numex::BigDec(numex::BigInt(-1), numex::BigInt(1)), type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdNumFloatZero(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<FloatLiteralAst> {
  // "0.0" as a "Self".
  const auto type = SizedNumericLiteralTypeNameOrError(sm, types, true);
  return FloatLiteralAst::FromBigVal(numex::BigDec(numex::BigInt(0), numex::BigInt(1)), type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdNumFloatOne(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<FloatLiteralAst> {
  // "1.0" as a "Self".
  const auto type = SizedNumericLiteralTypeNameOrError(sm, types, true);
  return FloatLiteralAst::FromBigVal(numex::BigDec(numex::BigInt(1), numex::BigInt(1)), type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdNumIntNegOne(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // "-1" as a "Self".
  const auto type = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(numex::BigInt(-1), type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdNumIntZero(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // "0" as a "Self".
  const auto type = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(numex::BigInt(0), type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdNumIntOne(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // "1" as a "Self".
  const auto type = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(numex::BigInt(1), type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdNumIntTwo(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // "2" as a "Self".
  const auto type = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(numex::BigInt(2), type);
}

auto spp::analyse::utils::comp_time_intrinsics::StdMemOpsSizeOf(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // Get the size of a type as an integer literal.
  const auto size = codegen::SizeOf(sm, *types[0]);
  auto tok = MakeUnique<TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(size));
  return MakeUnique<IntegerLiteralAst>(nullptr, std::move(tok), "uz");
}

auto spp::analyse::utils::comp_time_intrinsics::StdMemOpsAlignOf(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // Get the alignment of a type as an integer literal.
  const auto size = codegen::AlignOf(sm, *types[0]);
  auto tok = MakeUnique<TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(size));
  return MakeUnique<IntegerLiteralAst>(nullptr, std::move(tok), "uz");
}

// Every binary compound assignment is the same shape: run the operation, then overwrite the left literal with the
// result. The operations themselves differ and are written out above; only this last step repeats, so it is generated
// rather than copied eighteen times. The declarations stay written out in the interface, so each name is still
// searchable there and at its registration in "builtins.cpp".
#define SPP_CMP_ASSIGN_OP(name, lit_ty)                                \
  auto spp::analyse::utils::comp_time_intrinsics::StdIntrinsics##name##Assign(  \
    asts::lit_ty &lhs,                                                 \
    asts::lit_ty const &rhs)                                           \
    -> void {                                                          \
    AssignInPlace(lhs, StdIntrinsics##name(lhs, rhs));                 \
  }

SPP_CMP_ASSIGN_OP(Add, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(Sub, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(Mul, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(Div, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(Rem, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(BitShl, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(BitShr, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(BitIor, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(BitAnd, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(BitXor, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(Fadd, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(Fsub, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(Fmul, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(Fdiv, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(Frem, FloatLiteralAst)

#undef SPP_CMP_ASSIGN_OP
