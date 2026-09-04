module;
#include <spp/parse/macros.hpp>

module spp.analyse.utils.cmp_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
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
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
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
    /**
     * An "op_assign" intrinsic is its "op" applied in place: the result replaces what @p lhs holds. Seventeen of
     * these were written out longhand, each nine lines differing only in which "op" it called and which fields the
     * literal carries - exactly the shape that lets one copy drift away from the others unnoticed.
     * @param lhs The literal being assigned into.
     * @param result What the operation produced.
     */
    auto AssignInPlace(
      asts::IntegerLiteralAst &lhs,
      Unique<asts::IntegerLiteralAst> &&result)
      -> void {
      lhs.TokSign = std::move(result->TokSign);
      lhs.Val = std::move(result->Val);
      lhs.Type = std::move(result->Type);
    }

    auto AssignInPlace(
      asts::FloatLiteralAst &lhs,
      Unique<asts::FloatLiteralAst> &&result)
      -> void {
      lhs.TokSign = std::move(result->TokSign);
      lhs.IntVal = std::move(result->IntVal);
      lhs.FracVal = std::move(result->FracVal);
      lhs.Type = std::move(result->Type);
    }
  }
}

auto spp::analyse::utils::cmp_utils::SetCompTimeAttrValue(
  asts::ObjectInitializerAst const *object,
  asts::Ast const *attribute,
  Unique<asts::ExpressionAst> &&value,
  scopes::ScopeManager const *sm)
  -> void {
  // Firstly, we need to split the "attribute" as it may be a dotted path.
  auto attr_path = Vec<Shared<asts::IdentifierAst>>();
  while (true) {
    // Ensure we are looking at a postfix expression.
    const auto postfix = attribute->To<asts::PostfixExpressionAst>();
    if (postfix == nullptr) { break; }

    // Ensure the operator is a runtime member access.
    const auto member_access = postfix->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>();
    if (member_access == nullptr) { break; }

    // Push the member name to the path and continue down the lhs.
    attr_path.push_back(member_access->Name);
    attribute = postfix->Lhs.get();
  }

  // Reverse the attribute path to get the correct order.
  attr_path |= genex::actions::reverse;
  attr_path |= genex::actions::pop_front;

  // For each attribute, check if the initializer exists for tis attribute.
  // For example, if we have x.y.z = 5, and the types are x->X, y->Y, z->Z,
  //  we need to ensure that X has a Y initializer, which has a Z initializer.
  //  each inner initializer may exist, may not, so check and create if needed.
  auto current_obj_init = object;
  auto current_obj_type = object->Type;
  auto current_obj_sym = sm->CurrentScope->GetTypeSymbol(current_obj_type.get());

  for (auto const &attr_name : attr_path) {
    const auto is_final = attr_name == attr_path.Back();
    current_obj_type = current_obj_sym->LinkedScope->GetVarSymbol(attr_name.get())->Type;
    current_obj_sym = current_obj_sym->LinkedScope->GetTypeSymbol(current_obj_type.get());

    // Check if the attribute already exists in the current object initializer.
    const auto found = genex::contains(
      current_obj_init->ArgGroup->GetAllArgs(), *attr_name, [](auto const *x) -> decltype(auto) { return *x->Name; });
    if (found) {
      auto const &arg = **genex::find_if(
        current_obj_init->ArgGroup->GetAllArgs(), [&](auto const *x) { return *x->Name == *attr_name; });
      const auto obj = arg.Val->To<asts::ObjectInitializerAst>();

      // Case: attribute exists, but not as an object initializer. Replace it, but keep old object as the "else"
      // in the initializer, to use all of its other attributes.
      if (obj == nullptr) {
        const auto old_obj_init = current_obj_init;
        auto new_init = is_final
          ? std::move(value)
          : MakeUnique<asts::ObjectInitializerAst>(current_obj_sym->FqName(), nullptr);
        current_obj_init = new_init->To<asts::ObjectInitializerAst>();

        old_obj_init->ArgGroup->Args.EmplaceBack(
          MakeUnique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(new_init)));
        old_obj_init->ArgGroup->Args.EmplaceBack(
          asts::ObjectInitializerArgumentShorthandAst::CreateAutoFillArg(asts::AstClone(arg.Val)));
        continue;
      }

      // Case: attribute exists, and as an object initializer. In this case, just forward into the next
      // initializer.
      if (not is_final) {
        current_obj_init = obj;
        current_obj_type = current_obj_sym->LinkedScope->GetVarSymbol(attr_name.get())->Type;
        current_obj_sym = current_obj_sym->LinkedScope->GetTypeSymbol(current_obj_type.get());
        continue;
      }
      current_obj_init->ArgGroup->Args.EmplaceBack(
        MakeUnique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(value)));
    }

    // Case: attribute does not exist, so create a new object initializer for it.
    else {
      const auto old_obj_init = current_obj_init;
      auto new_init = is_final
        ? std::move(value)
        : MakeUnique<asts::ObjectInitializerAst>(current_obj_sym->FqName(), nullptr);
      current_obj_init = new_init->To<asts::ObjectInitializerAst>();

      old_obj_init->ArgGroup->Args.EmplaceBack(
        MakeUnique<asts::ObjectInitializerArgumentKeywordAst>(AstCloneShared(attr_name), nullptr, std::move(new_init)));
    }
  }
}

auto spp::analyse::utils::cmp_utils::GetCompTimeAttrValue(
  asts::ObjectInitializerAst const *object,
  asts::IdentifierAst const *attribute)
  -> Unique<asts::ExpressionAst> {
  // Check each argument in the object initializer for the target attribute.
  for (auto const &arg : object->ArgGroup->GetAllArgs()) {
    if (*arg->Name == *attribute) { return asts::AstClone(arg->Val); }
  }
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_add(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform addition on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() + rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sub(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform subtraction on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() - rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_mul(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform multiplication on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() * rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sdiv(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed division on two integer literals, exactly.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() / rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_udiv(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned division on two integer literals, exactly.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() / rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_srem(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed remainder on two integer literals, exactly.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() % rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_urem(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned remainder on two integer literals, exactly.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() % rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sneg(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed negation on an integer literal.
  return asts::IntegerLiteralAst::FromBigVal(-val.BigVal(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shl(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise left shift on two integer literals. S++ forces U32 too (safe).
  return asts::IntegerLiteralAst::FromWrappedBigVal(
    lhs.BigVal() << rhs.CppVal<std::uint32_t>(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shr(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise right shift on two integer literals. S++ forces U32 too (safe).
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() >> rhs.CppVal<std::uint32_t>(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_ior(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise OR on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() | rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_and(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise AND on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() & rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_xor(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise XOR on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal() ^ rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise NOT on an integer literal.
  return asts::IntegerLiteralAst::FromBigVal(~val.BigVal(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not_assign(
  asts::IntegerLiteralAst &lhs)
  -> void {
  // Perform bitwise NOT assignment on an integer literal. Written out rather than generated below because it is the
  // one unary assignment; it used to hand-roll the field copy that "AssignInPlace" does, and had drifted from it.
  AssignInPlace(lhs, std_intrinsics_bit_not(lhs));
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_abs(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform absolute value on an integer literal.
  const auto value = val.BigVal();
  return asts::IntegerLiteralAst::FromBigVal(val.BigVal().Abs(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_eq(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform equality comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() == rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_oeq(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered equality comparison on two float literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() == rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ne(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform inequality comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() != rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_one(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered inequality comparison on two float literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() != rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_slt(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed less-than comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() < rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ult(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned less-than comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() < rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_olt(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered less-than comparison on two float literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() < rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sle(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed less-than-or-equal comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() <= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ule(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned less-than-or-equal comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() <= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ole(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered less-than-or-equal comparison on two float literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() <= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sgt(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed greater-than comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() > rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ugt(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned greater-than comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() > rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ogt(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered greater-than comparison on two float literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() > rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sge(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed greater-than-or-equal comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() >= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_uge(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned greater-than-or-equal comparison on two integer literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() >= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_oge(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered greater-than-or-equal comparison on two float literals.
  return asts::BooleanLiteralAst::FromCppVal(lhs.BigVal() >= rhs.BigVal());
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_max_val(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Return the maximum value for the type of the integer literal.
  return asts::IntegerLiteralAst::FromBigVal(asts::IntegerLiteralAst::kBounds.at(val.Type).first, val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_min_val(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Return the minimum value for the type of the integer literal.
  return asts::IntegerLiteralAst::FromBigVal(asts::IntegerLiteralAst::kBounds.at(val.Type).second, val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_smax(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed maximum on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal().Max(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_umax(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned maximum on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal().Max(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_smin(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed minimum on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal().Min(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_umin(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned minimum on two integer literals.
  return asts::IntegerLiteralAst::FromBigVal(lhs.BigVal().Min(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_scmp(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed comparison on two integer literals.
  const auto cmp = lhs.BigVal() <=> rhs.BigVal();
  const auto res = std::is_gt(cmp) - std::is_lt(cmp);
  return asts::IntegerLiteralAst::FromBigVal(numex::BigInt(res), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ucmp(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned comparison on two integer literals.
  const auto cmp = lhs.BigVal() <=> rhs.BigVal();
  const auto res = std::is_gt(cmp) - std::is_lt(cmp);
  return asts::IntegerLiteralAst::FromBigVal(numex::BigInt(res), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fadd(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform addition on two float literals. The arithmetic is exact rather than done in a fixed-width C++ float: a
  // result the type cannot hold has to stay a value the caller can reject, not become an infinity.
  return asts::FloatLiteralAst::FromBigVal(lhs.BigVal() + rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fsub(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform subtraction on two float literals. The arithmetic is exact rather than done in a fixed-width C++ float: a
  // result the type cannot hold has to stay a value the caller can reject, not become an infinity.
  return asts::FloatLiteralAst::FromBigVal(lhs.BigVal() - rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmul(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform multiplication on two float literals. The arithmetic is exact rather than done in a fixed-width C++ float: a
  // result the type cannot hold has to stay a value the caller can reject, not become an infinity.
  return asts::FloatLiteralAst::FromBigVal(lhs.BigVal() * rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fdiv(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform division on two float literals.
  return asts::FloatLiteralAst::FromBigVal(lhs.BigVal() / rhs.BigVal(), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_frem(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform remainder on two float literals.
  return asts::FloatLiteralAst::FromBigVal(lhs.BigVal().Fmod(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fneg(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform negation on a float literal, exactly.
  return asts::FloatLiteralAst::FromBigVal(-val.BigVal(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fabs(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform absolute value on a float literal, exactly.
  return asts::FloatLiteralAst::FromBigVal(val.BigVal().Abs(), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmax_val(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Get the largest finite value that this float type can hold.
  return asts::FloatLiteralAst::FromBigVal(asts::FloatLiteralAst::kBounds.at(val.Type).first, val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmin_val(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Get the most negative finite value that this float type can hold.
  return asts::FloatLiteralAst::FromBigVal(asts::FloatLiteralAst::kBounds.at(val.Type).second, val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmax(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform maximum on two float literals.
  return asts::FloatLiteralAst::FromBigVal(lhs.BigVal().Max(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmin(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform minimum on two float literals.
  return asts::FloatLiteralAst::FromBigVal(lhs.BigVal().Min(rhs.BigVal()), lhs.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ffloor(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform floor operation on a float literal.
  return asts::FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Floor()), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fceil(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform ceiling operation on a float literal.
  return asts::FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Ceil()), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ftrunc(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform truncation operation on a float literal.
  return asts::FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Trunc()), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fround(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform round operation on a float literal.
  return asts::FloatLiteralAst::FromBigVal(numex::BigDec(val.BigVal().Trunc()), val.Type);
}

auto spp::analyse::utils::cmp_utils::std_num_float_neg_one()
  -> Unique<asts::FloatLiteralAst> {
  // Get "-1.0" as a constant.
  constexpr auto value = "-1.0";
  auto flt = INJECT_CODE(value, parse_literal_float);
  return flt;
}

auto spp::analyse::utils::cmp_utils::std_num_float_zero()
  -> Unique<asts::FloatLiteralAst> {
  // Get "0.0" as a constant.
  constexpr auto value = "0.0";
  auto num = INJECT_CODE(value, parse_literal_float);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_float_one()
  -> Unique<asts::FloatLiteralAst> {
  // Get "1.0" as a constant.
  constexpr auto value = "1.0";
  auto num = INJECT_CODE(value, parse_literal_float);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_neg_one()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "-1" as a constant.
  constexpr auto value = "-1";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_zero()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "0" as a constant.
  constexpr auto value = "0";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_one()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "1" as a constant.
  constexpr auto value = "1";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_two()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "2" as a constant.
  constexpr auto value = "2";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_mem_ops_size_of(
  scopes::ScopeManager const &sm,
  Vec<asts::TypeAst*> const &types)
  -> Unique<asts::IntegerLiteralAst> {
  // Get the size of a type as an integer literal.
  const auto size = codegen::SizeOf(sm, *types[0]);
  auto tok = MakeUnique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(size));
  return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(tok), "uz");
}

auto spp::analyse::utils::cmp_utils::std_mem_ops_align_of(
  scopes::ScopeManager const &sm,
  Vec<asts::TypeAst*> const &types)
  -> Unique<asts::IntegerLiteralAst> {
  // Get the alignment of a type as an integer literal.
  const auto size = codegen::AlignOf(sm, *types[0]);
  auto tok = MakeUnique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(size));
  return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(tok), "uz");
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
SPP_CMP_ASSIGN_OP(sdiv, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(udiv, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(srem, IntegerLiteralAst)
SPP_CMP_ASSIGN_OP(urem, IntegerLiteralAst)
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
