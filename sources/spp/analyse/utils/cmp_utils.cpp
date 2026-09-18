module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.analyse.utils.cmp_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.object_initializer_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_size;
import spp.lex.lexer;
import spp.lex.tokens;
import spp.parse.parser_spp;
import spp.utils.strings;
import genex;
import numex.big_dec;
import numex.big_int;

namespace spp::analyse::utils::cmp_utils {
  namespace {
    auto SizedNumericLiteralTypeName(TypeSymbol const *sym) -> Str {
      // The width and signedness are the sized type's own bindings of "w" and "signed", read from the instantiation as
      // its type arguments would be - not parsed out of how its name happens to be written.
      if (sym == nullptr) { return Str(); }
      const auto width_val = sym->BoundCompArg("w");
      const auto width_lit = width_val != nullptr ? width_val->To<IntegerLiteralAst>() : nullptr;
      if (width_lit == nullptr) { return Str(); }
      const auto width = width_lit->Val->ToString();

      // A floating-point type has a width and nothing else; an integer one also says whether it is signed.
      const auto signed_val = sym->BoundCompArg("signed");
      if (signed_val == nullptr) { return "f" + width; }
      const auto signed_lit = signed_val->To<BooleanLiteralAst>();
      const auto is_signed = signed_lit != nullptr and signed_lit->CppVal();
      return (is_signed ? "s" : "u") + width;
    }

    auto SizedNumericLiteralTypeNameOrError(
      ScopeManager const &sm, Vec<Shared<TypeRef>> const &types, const bool is_float) -> Str {
      // Get the type suffix for this integer or float
      // type, and check that it is known.
      SPP_ASSERT(not types.IsEmpty());
      const auto name = SizedNumericLiteralTypeName(types[0]->Sym);
      const auto known = is_float
        ? FloatLiteralAst::kBounds.contains(name)
        : IntegerLiteralAst::kBounds.contains(name);

      // ICE failsafe to prevent worse downstream errors.
      if (not known) {
        SPP_ASSERT(types[0]->Sym != nullptr);
        const auto type = types[0]->Sym->FqName();
        Raise<errors::SppInternalCompilerError>({sm.CurrentScope}, ERR_ARGS(*type, "no numeric bounds for this type"));
      }
      return name;
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

auto spp::analyse::utils::cmp_utils::SetCompTimeAttrValue(
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
  auto current_sym = sm->CurrentScope->GetTypeSymbol(object->Type.get());
  for (auto i = 0uz; i < attr_path.Len(); ++i) {
    auto const &attr_name = attr_path[i];
    auto *const arg = FindNamedArg(*current, *attr_name);
    if (i + 1 == attr_path.Len()) {
      if (arg != nullptr) { arg->Val = std::move(value); }
      else {
        current->ArgGroup->Args.EmplaceBack(MakeUnique<ObjectInitializerArgumentKeywordAst>(
          AstCloneShared(attr_name), nullptr, std::move(value)));
      }
      return;
    }

    const auto attr_type = current_sym->LinkedScope->GetVarSymbol(attr_name.get())->Type;
    current_sym = current_sym->LinkedScope->GetTypeSymbol(attr_type.get());
    if (const auto inner = arg != nullptr ? arg->Val->To<ObjectInitializerAst>() : nullptr; inner != nullptr) {
      current = inner;
      continue;
    }

    auto new_init = MakeUnique<ObjectInitializerAst>(current_sym->FqName(), nullptr);
    const auto new_init_ptr = new_init.get();
    if (arg != nullptr) {
      new_init->ArgGroup->Args.EmplaceBack(
        ObjectInitializerArgumentShorthandAst::CreateAutoFillArg(std::move(arg->Val)));
      arg->Val = std::move(new_init);
    }
    else {
      current->ArgGroup->Args.EmplaceBack(MakeUnique<ObjectInitializerArgumentKeywordAst>(
        AstCloneShared(attr_name), nullptr, std::move(new_init)));
    }
    current = new_init_ptr;
  }
}

auto spp::analyse::utils::cmp_utils::GetCompTimeAttrValue(
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

auto spp::analyse::utils::cmp_utils::FoldCompExpr(
  ExpressionAst const &expr,
  Scope const &scope)
  -> Unique<ExpressionAst> {
  using lex::SppTokenType;

  // A literal is its own value, spelled canonically:
  // "0x10_uz" and "16_uz" are one value.
  if (const auto lit = expr.To<IntegerLiteralAst>(); lit != nullptr) {
    return IntegerLiteralAst::FromBigVal(lit->BigVal(), lit->Type);
  }
  if (const auto lit = expr.To<BooleanLiteralAst>(); lit != nullptr) {
    return BooleanLiteralAst::FromCppVal(lit->CppVal());
  }
  if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    return FoldCompExpr(*paren->Expr, scope);
  }

  // A comp generic is the value it is bound to. An unbound
  // one, or one bound to another generic, is not closed.
  if (const auto id = expr.To<IdentifierAst>(); id != nullptr) {
    const auto var = scope.GetVarSymbol(id);
    if (var == nullptr or not var->IsCompGeneric()) { return nullptr; }
    const auto bound = var->BoundCompValue();
    if (bound == nullptr or bound->To<IdentifierAst>() != nullptr) { return nullptr; }
    return FoldCompExpr(*bound, scope);
  }

  // A binary operation over two folded values, by the
  // comp-time intrinsic its operator maps to.
  const auto bin = expr.To<BinaryExpressionAst>();
  if (bin == nullptr) { return nullptr; }
  const auto lhs = FoldCompExpr(*bin->Lhs, scope);
  const auto rhs = lhs != nullptr ? FoldCompExpr(*bin->Rhs, scope) : nullptr;
  if (lhs == nullptr or rhs == nullptr) { return nullptr; }
  const auto op = bin->TokOp->TokenType;

  const auto lhs_bool = lhs->To<BooleanLiteralAst>();
  const auto rhs_bool = rhs->To<BooleanLiteralAst>();
  if (lhs_bool != nullptr and rhs_bool != nullptr) {
    if (op == SppTokenType::TK_EQ) { return BooleanLiteralAst::FromCppVal(lhs_bool->CppVal() == rhs_bool->CppVal()); }
    if (op == SppTokenType::TK_NE) { return BooleanLiteralAst::FromCppVal(lhs_bool->CppVal() != rhs_bool->CppVal()); }
    return nullptr;
  }

  // An unsuffixed literal takes the other side's type,
  // as it does in an expression ("n + 1" with "n: USize");
  // two different types do not mix.
  const auto lhs_int = lhs->To<IntegerLiteralAst>();
  const auto rhs_int = rhs->To<IntegerLiteralAst>();
  if (lhs_int == nullptr or rhs_int == nullptr) { return nullptr; }
  if (not lhs_int->Type.empty() and not rhs_int->Type.empty() and lhs_int->Type != rhs_int->Type) { return nullptr; }
  const auto type = not lhs_int->Type.empty() ? lhs_int->Type : rhs_int->Type;
  const auto l = IntegerLiteralAst::FromBigVal(lhs_int->BigVal(), type);
  const auto r = IntegerLiteralAst::FromBigVal(rhs_int->BigVal(), type);
  const auto by_zero = r->Val->TokenData == "0";

  switch (op) {
    case SppTokenType::TK_ADD: return std_intrinsics_add(*l, *r);
    case SppTokenType::TK_SUB: return std_intrinsics_sub(*l, *r);
    case SppTokenType::TK_MUL: return std_intrinsics_mul(*l, *r);
    case SppTokenType::TK_DIV: return by_zero ? nullptr : std_intrinsics_div(*l, *r);
    case SppTokenType::TK_REM: return by_zero ? nullptr : std_intrinsics_rem(*l, *r);
    case SppTokenType::TK_BIT_IOR: return std_intrinsics_bit_ior(*l, *r);
    case SppTokenType::TK_BIT_AND: return std_intrinsics_bit_and(*l, *r);
    case SppTokenType::TK_BIT_XOR: return std_intrinsics_bit_xor(*l, *r);
    case SppTokenType::TK_BIT_SHL: return std_intrinsics_bit_shl(*l, *r);
    case SppTokenType::TK_BIT_SHR: return std_intrinsics_bit_shr(*l, *r);
    case SppTokenType::TK_EQ: return std_intrinsics_eq(*l, *r);
    case SppTokenType::TK_NE: return std_intrinsics_ne(*l, *r);
    case SppTokenType::TK_LT: return std_intrinsics_lt(*l, *r);
    case SppTokenType::TK_LE: return std_intrinsics_le(*l, *r);
    case SppTokenType::TK_GT: return std_intrinsics_gt(*l, *r);
    case SppTokenType::TK_GE: return std_intrinsics_ge(*l, *r);
    default: return nullptr;
  }
}

auto spp::analyse::utils::cmp_utils::StampCompGenerics(
  ExpressionAst const &expr,
  Scope const &scope)
  -> void {
  // Move inside a comptime parenthesis expression.
  if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    StampCompGenerics(*paren->Expr, scope);
    return;
  }

  // Handle binary expressions using the nodes.
  if (const auto bin = expr.To<BinaryExpressionAst>(); bin != nullptr) {
    if (bin->Lhs != nullptr) { StampCompGenerics(*bin->Lhs, scope); }
    if (bin->Rhs != nullptr) { StampCompGenerics(*bin->Rhs, scope); }
    return;
  }

  // Handle identifiers using their symbols.
  const auto id = expr.To<IdentifierAst>();
  if (id == nullptr or id->Stamp() != nullptr) { return; }
  if (auto *const sym = scope.GetVarSymbol(id); sym != nullptr
    and sym->Kind == VariableKind::GenericCompParam and sym->ParamId != 0) {
    id->SetStamp(sym);
  }
}

auto spp::analyse::utils::cmp_utils::CompExprIdentity(
  ExpressionAst const &expr, Scope const &scope, Str &out) -> void {
  // A closed value is what it folds to: "1_uz + 1_uz", "n + 1_uz"
  // with "n" bound to "1_uz", and "2_uz" are one value.
  if (const auto folded = FoldCompExpr(expr, scope); folded != nullptr) {
    out += 'V';
    out += folded->ToString();
    return;
  }
  if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    CompExprIdentity(*paren->Expr, scope, out);
    return;
  }

  // A comp generic is the parameter at the end of its chain of
  // bindings to other generics - an inherited "w" bound to the
  // block's own "w" is that "w" - however it is spelled here.
  // A binding to a value that does not fold is that value.
  if (const auto id = expr.To<IdentifierAst>(); id != nullptr) {
    auto const *var = scope.GetVarSymbol(id);
    if (var == nullptr or not var->IsCompGeneric()) {
      out += 'V';
      out += expr.ToString();
      return;
    }
    for (auto depth = 0; depth < 16; ++depth) {
      const auto bound = var->BoundCompValue();
      if (bound == nullptr) { break; }
      const auto bound_id = bound->To<IdentifierAst>();
      if (bound_id == nullptr) {
        out += 'V';
        out += bound->ToString();
        return;
      }
      const auto next = scope.GetVarSymbol(bound_id);
      if (next == nullptr or next == var or not next->IsCompGeneric()) { break; }
      var = next;
    }
    const auto param_id = var->ParamId != 0 ? var->ParamId : var->BindsParamId;
    auto digits = std::array<char, 20>();
    const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), param_id);
    out += 'C';
    out.append(digits.data(), written.ptr);
    return;
  }

  // An operation is its operands' identities under its operator, bracketed so precedence is explicit.
  if (const auto bin = expr.To<BinaryExpressionAst>(); bin != nullptr and bin->Lhs != nullptr and bin->Rhs !=
    nullptr) {
    out += '(';
    CompExprIdentity(*bin->Lhs, scope, out);
    out += ' ';
    out += bin->TokOp->TokenData;
    out += ' ';
    CompExprIdentity(*bin->Rhs, scope, out);
    out += ')';
    return;
  }
  out += 'V';
  out += expr.ToString();
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_add(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform addition on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() + rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sub(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform subtraction on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() - rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_mul(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform multiplication on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() * rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_div(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform division on two integer literals, exactly.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() / rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_rem(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform remainder on two integer literals, exactly.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() % rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sneg(
  IntegerLiteralAst const &val)
  -> Unique<IntegerLiteralAst> {
  // Perform signed negation on an integer literal.
  return IntegerLiteralAst::FromBigVal(-val.BigVal(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shl(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise left shift on two integer literals. S++ forces U32 too (safe).
  return IntegerLiteralAst::FromWrappedBigVal(
    lhs.BigVal() << rhs.CppVal<std::uint32_t>(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shr(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise right shift on two integer literals. S++ forces U32 too (safe).
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() >> rhs.CppVal<std::uint32_t>(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_ior(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise OR on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() | rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_and(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise AND on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() & rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_xor(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise XOR on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal() ^ rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not(
  IntegerLiteralAst const &val)
  -> Unique<IntegerLiteralAst> {
  // Perform bitwise NOT on an integer literal.
  return IntegerLiteralAst::FromBigVal(~val.BigVal(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not_assign(
  IntegerLiteralAst &lhs)
  -> void {
  // Perform bitwise NOT assignment on an integer literal. Written out rather than generated below because it is the
  // one unary assignment; it used to hand-roll the field copy that "AssignInPlace" does, and had drifted from it.
  AssignInPlace(lhs, std_intrinsics_bit_not(lhs));
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_abs(
  IntegerLiteralAst const &val)
  -> Unique<IntegerLiteralAst> {
  // Perform absolute value on an integer literal.
  return IntegerLiteralAst::FromBigVal(val.BigVal().Abs(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_eq(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform equality comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() == rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_oeq(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered equality comparison on two float literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() == rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ne(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform inequality comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() != rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_one(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered inequality comparison on two float literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() != rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_lt(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform less-than comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() < rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_olt(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered less-than comparison on two float literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() < rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_le(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform less-than-or-equal comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() <= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ole(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered less-than-or-equal comparison on two float literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() <= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_gt(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform greater-than comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() > rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ogt(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered greater-than comparison on two float literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() > rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ge(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform greater-than-or-equal comparison on two integer literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() >= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_oge(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<BooleanLiteralAst> {
  // Perform ordered greater-than-or-equal comparison on two float literals.
  return BooleanLiteralAst::FromCppVal(lhs.BigVal() >= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_max_val(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // The highest value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(
    IntegerLiteralAst::kBounds.at(name).second, name);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_min_val(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // The lowest value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, false);
  return IntegerLiteralAst::FromBigVal(
    IntegerLiteralAst::kBounds.at(name).first, name);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_max(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform maximum on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal().Max(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_min(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform minimum on two integer literals.
  return IntegerLiteralAst::FromBigVal(lhs.BigVal().Min(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_cmp(
  IntegerLiteralAst const &lhs,
  IntegerLiteralAst const &rhs)
  -> Unique<IntegerLiteralAst> {
  // Perform three-way comparison on two integer literals.
  const auto cmp = lhs.BigVal() <=> rhs.BigVal();
  const auto res = std::is_gt(cmp) - std::is_lt(cmp);
  return IntegerLiteralAst::FromBigVal(numex::BigInt(res), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fadd(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // Perform addition on two float literals. The arithmetic is exact rather than done in a fixed-width C++ float: a
  // result the type cannot hold has to stay a value the caller can reject, not become an infinity.
  return FloatLiteralAst::FromBigVal(lhs.BigVal() + rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fsub(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // Perform subtraction on two float literals. The arithmetic is exact rather than done in a fixed-width C++ float: a
  // result the type cannot hold has to stay a value the caller can reject, not become an infinity.
  return FloatLiteralAst::FromBigVal(lhs.BigVal() - rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmul(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // Perform multiplication on two float literals. The arithmetic is exact rather than done in a fixed-width C++ float: a
  // result the type cannot hold has to stay a value the caller can reject, not become an infinity.
  return FloatLiteralAst::FromBigVal(lhs.BigVal() * rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fdiv(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // Perform division on two float literals.
  return FloatLiteralAst::FromBigVal(lhs.BigVal() / rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_frem(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // Perform remainder on two float literals.
  return FloatLiteralAst::FromBigVal(lhs.BigVal().Fmod(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fneg(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // Perform negation on a float literal, exactly.
  return FloatLiteralAst::FromBigVal(-val.BigVal(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fabs(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // Perform absolute value on a float literal, exactly.
  return FloatLiteralAst::FromBigVal(val.BigVal().Abs(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmax_val(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<FloatLiteralAst> {
  // The largest finite value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, true);
  return FloatLiteralAst::FromBigVal(FloatLiteralAst::kBounds.at(name).second, name);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmin_val(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<FloatLiteralAst> {
  // The most negative finite value the type argument can represent.
  const auto name = SizedNumericLiteralTypeNameOrError(sm, types, true);
  return FloatLiteralAst::FromBigVal(FloatLiteralAst::kBounds.at(name).first, name);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmax(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // Perform maximum on two float literals.
  return FloatLiteralAst::FromBigVal(lhs.BigVal().Max(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmin(
  FloatLiteralAst const &lhs,
  FloatLiteralAst const &rhs)
  -> Unique<FloatLiteralAst> {
  // Perform minimum on two float literals.
  return FloatLiteralAst::FromBigVal(lhs.BigVal().Min(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ffloor(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // Perform floor operation on a float literal.
  return FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Floor(), numex::BigInt(1)), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fceil(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // Perform ceiling operation on a float literal.
  return FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Ceil(), numex::BigInt(1)), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ftrunc(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // Perform truncation operation on a float literal.
  return FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Trunc(), numex::BigInt(1)), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fround(
  FloatLiteralAst const &val)
  -> Unique<FloatLiteralAst> {
  // Perform round operation on a float literal: half away from zero, as "llvm.round" does at runtime.
  return FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Round(), numex::BigInt(1)), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_num_float_neg_one()
  -> Unique<FloatLiteralAst> {
  // Get "-1.0" as a constant.
  constexpr auto value = "-1.0";
  auto flt = INJECT_CODE(value, parse_literal_float);
  return flt;
}

auto spp::analyse::utils::cmp_utils::std_num_float_zero()
  -> Unique<FloatLiteralAst> {
  // Get "0.0" as a constant.
  constexpr auto value = "0.0";
  auto num = INJECT_CODE(value, parse_literal_float);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_float_one()
  -> Unique<FloatLiteralAst> {
  // Get "1.0" as a constant.
  constexpr auto value = "1.0";
  auto num = INJECT_CODE(value, parse_literal_float);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_neg_one()
  -> Unique<IntegerLiteralAst> {
  // Get "-1" as a constant.
  constexpr auto value = "-1";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_zero()
  -> Unique<IntegerLiteralAst> {
  // Get "0" as a constant.
  constexpr auto value = "0";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_one()
  -> Unique<IntegerLiteralAst> {
  // Get "1" as a constant.
  constexpr auto value = "1";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_two()
  -> Unique<IntegerLiteralAst> {
  // Get "2" as a constant.
  constexpr auto value = "2";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_mem_ops_size_of(
  ScopeManager const &sm,
  Vec<Shared<TypeRef>> const &types)
  -> Unique<IntegerLiteralAst> {
  // Get the size of a type as an integer literal.
  const auto size = codegen::SizeOf(sm, *types[0]);
  auto tok = MakeUnique<TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(size));
  return MakeUnique<IntegerLiteralAst>(nullptr, std::move(tok), "uz");
}

auto spp::analyse::utils::cmp_utils::std_mem_ops_align_of(
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
  auto spp::analyse::utils::cmp_utils::std_intrinsics_##name##_assign( \
    asts::lit_ty &lhs,                                                 \
    asts::lit_ty const &rhs)                                           \
    -> void {                                                          \
    AssignInPlace(lhs, std_intrinsics_##name(lhs, rhs));               \
  }

SPP_CMP_ASSIGN_OP(add, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(sub, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(mul, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(div, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(rem, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(bit_shl, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(bit_shr, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(bit_ior, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(bit_and, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(bit_xor, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(fadd, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(fsub, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(fmul, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(fdiv, FloatLiteralAst)
SPP_CMP_ASSIGN_OP(frem, FloatLiteralAst)

#undef SPP_CMP_ASSIGN_OP
