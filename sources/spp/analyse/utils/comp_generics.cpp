module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.comp_generics;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comptime_intrinsics;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_size;
import spp.lex.lexer;
import spp.lex.tokens;
import spp.parse.parser_spp;
import genex;
import std;
import numex.big_dec;
import numex.big_int;

auto spp::analyse::utils::comp_generics::FoldCompExpr(
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

  // A pack ("cmp ..ns") is a tuple of values, closed when every element is, as a type pack's tuple is.
  if (const auto tup = expr.To<TupleLiteralAst>(); tup != nullptr) {
    auto elems = Vec<Unique<ExpressionAst>>();
    for (auto const &elem : tup->Elems) {
      auto folded = FoldCompExpr(*elem, scope);
      if (folded == nullptr) { return nullptr; }
      elems.EmplaceBack(std::move(folded));
    }
    return MakeUnique<TupleLiteralAst>(nullptr, std::move(elems), nullptr);
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
    case SppTokenType::TK_ADD: return comptime_intrinsics::std_intrinsics_add(*l, *r);
    case SppTokenType::TK_SUB: return comptime_intrinsics::std_intrinsics_sub(*l, *r);
    case SppTokenType::TK_MUL: return comptime_intrinsics::std_intrinsics_mul(*l, *r);
    case SppTokenType::TK_DIV: return by_zero ? nullptr : comptime_intrinsics::std_intrinsics_div(*l, *r);
    case SppTokenType::TK_REM: return by_zero ? nullptr : comptime_intrinsics::std_intrinsics_rem(*l, *r);
    case SppTokenType::TK_BIT_IOR: return comptime_intrinsics::std_intrinsics_bit_ior(*l, *r);
    case SppTokenType::TK_BIT_AND: return comptime_intrinsics::std_intrinsics_bit_and(*l, *r);
    case SppTokenType::TK_BIT_XOR: return comptime_intrinsics::std_intrinsics_bit_xor(*l, *r);
    case SppTokenType::TK_BIT_SHL: return comptime_intrinsics::std_intrinsics_bit_shl(*l, *r);
    case SppTokenType::TK_BIT_SHR: return comptime_intrinsics::std_intrinsics_bit_shr(*l, *r);
    case SppTokenType::TK_EQ: return comptime_intrinsics::std_intrinsics_eq(*l, *r);
    case SppTokenType::TK_NE: return comptime_intrinsics::std_intrinsics_ne(*l, *r);
    case SppTokenType::TK_LT: return comptime_intrinsics::std_intrinsics_lt(*l, *r);
    case SppTokenType::TK_LE: return comptime_intrinsics::std_intrinsics_le(*l, *r);
    case SppTokenType::TK_GT: return comptime_intrinsics::std_intrinsics_gt(*l, *r);
    case SppTokenType::TK_GE: return comptime_intrinsics::std_intrinsics_ge(*l, *r);
    default: return nullptr;
  }
}

auto spp::analyse::utils::comp_generics::AnyCompName(
  ExpressionAst const &expr, std::function<bool(IdentifierAst const &)> const &pred) -> bool {
  if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    return AnyCompName(*paren->Expr, pred);
  }
  if (const auto bin = expr.To<BinaryExpressionAst>(); bin != nullptr) {
    return (bin->Lhs != nullptr and AnyCompName(*bin->Lhs, pred)) or (bin->Rhs != nullptr and AnyCompName(*bin->Rhs, pred));
  }
  if (const auto tup = expr.To<TupleLiteralAst>(); tup != nullptr) {
    return genex::any_of(tup->Elems, [&](auto const &elem) { return AnyCompName(*elem, pred); });
  }
  const auto id = expr.To<IdentifierAst>();
  return id != nullptr and pred(*id);
}

auto spp::analyse::utils::comp_generics::NamesCompParam(
  ExpressionAst const &expr,
  Scope const &scope)
  -> bool {
  return AnyCompName(expr, [&scope](IdentifierAst const &id) {
    auto const *const sym = scope.GetVarSymbol(&id);
    return sym != nullptr and sym->Kind == VariableKind::GenericCompParam;
  });
}

auto spp::analyse::utils::comp_generics::RecordCompGenerics(
  ExpressionAst const &expr,
  Scope const &scope)
  -> void {
  // Every name, nested ones too. A parameter records itself, and a binding the parameter it binds, as a type argument
  // naming either does ("GenericArgumentAst::AnalyseTypeVal").
  static_cast<void>(AnyCompName(expr, [&scope](IdentifierAst const &id) {
    if (id.WrittenParam() != 0) { return false; }
    if (auto *const sym = scope.GetVarSymbol(&id); sym != nullptr) {
      if (sym->Kind == VariableKind::GenericCompParam and sym->ParamId != 0) { id.SetWrittenParam(sym->ParamId); }
      else if (sym->Kind == VariableKind::GenericCompArg and sym->BindsParamId != 0) {
        id.SetWrittenParam(sym->BindsParamId);
      }
    }
    return false;
  }));
}

auto spp::analyse::utils::comp_generics::CompExprIdentity(
  ExpressionAst const &expr, Scope const &scope, Str &out) -> void {
  // A pack is its elements' identities, each on its own, so a parameter among them is named by identity (and read, and
  // substituted, as an element) however the pack is spelled or bound: "(n, 1_uz)" with "n" bound to "1_uz" is
  // "P(V1_uz, V1_uz)", as "(1_uz, 1_uz)" is ("scopes::CompPackElements" reads it back).
  if (const auto tup = expr.To<TupleLiteralAst>(); tup != nullptr) {
    out += "P(";
    for (auto i = 0uz; i < tup->Elems.Len(); ++i) {
      if (i != 0) { out += ", "; }
      CompExprIdentity(*tup->Elems[i], scope, out);
    }
    out += ')';
    return;
  }

  // A closed value is what it folds to: "1_uz + 1_uz", "n + 1_uz"
  // with "n" bound to "1_uz", and "2_uz" are one value.
  if (const auto folded = FoldCompExpr(expr, scope); folded != nullptr) {
    // A pack folds to a pack, which is named by its elements.
    if (folded->To<TupleLiteralAst>() != nullptr) {
      CompExprIdentity(*folded, scope, out);
      return;
    }
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
      if (bound_id == nullptr and bound->To<TupleLiteralAst>() != nullptr) {
        CompExprIdentity(*bound, scope, out);
        return;
      }
      if (bound_id == nullptr) {
        out += 'V';
        out += bound->ToString();
        return;
      }
      const auto next = scope.GetVarSymbol(bound_id);
      if (next == nullptr or next == var or not next->IsCompGeneric()) { break; }
      var = next;
    }
    const auto param_id = var->ParamIdentity();
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

auto spp::analyse::utils::comp_generics::CompIdentityOfSym(
  VariableSymbol const &sym, Scope const &scope, Str &out) -> void {
  if (auto const *const bound = sym.BoundCompValue(); bound != nullptr) {
    CompExprIdentity(*bound, scope, out);
    return;
  }
  out += 'C';
  out += std::to_string(sym.ParamIdentity());
}

auto spp::analyse::utils::comp_generics::ResolveCompArg(
  ExpressionAst const &expr,
  Scope const &scope)
  -> Unique<ExpressionAst> {
  if (auto folded = FoldCompExpr(expr, scope); folded != nullptr) { return folded; }
  auto const *const id = expr.To<IdentifierAst>();
  auto const *const var = id != nullptr ? scope.GetVarSymbol(id) : nullptr;
  auto const *const bound = var != nullptr ? var->BoundCompValue() : nullptr;
  return bound != nullptr ? AstClone(bound) : nullptr;
}
