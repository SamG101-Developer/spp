module;
#include <spp/macros.hpp>

#define INVOKE_PATTERN(...)                                                                           \
  if constexpr (std::is_same_v<Ret, void>) {                                                          \
    fn(__VA_OPT__(__VA_ARGS__, ) dynamic_cast<std::remove_reference_t<Args>&>(*args[I])...);          \
    return nullptr;                                                                                   \
  }                                                                                                   \
  else {                                                                                              \
    auto v = fn(__VA_OPT__(__VA_ARGS__, ) dynamic_cast<std::remove_reference_t<Args>&>(*args[I])...); \
    return v;                                                                                         \
  }

export module spp.analyse.utils.cmp_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import genex;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct BooleanLiteralAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FloatLiteralAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct IntegerLiteralAst);
use(spp::asts, struct ObjectInitializerAst);
use(spp::asts, struct TypeAst);

namespace spp {
  template <bool HasGnTypeArgs, bool HasGnCompArgs, typename Ret, typename... Args>
  struct DetermineCmpFuncSig_ {
    using Type = void;
  };

  template <typename Ret, typename... Args>
  struct DetermineCmpFuncSig_<true, false, Ret, Args...> {
    using Type = Ret(*)(
      ScopeManager const &,
      decltype(meta::CompilerMetaData::CmpGnTypeArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCmpFuncSig_<false, true, Ret, Args...> {
    using Type = Ret(*)(
      ScopeManager const &,
      decltype(meta::CompilerMetaData::CmpGnCompArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCmpFuncSig_<true, true, Ret, Args...> {
    using Type = Ret(*)(
      ScopeManager const &,
      decltype(meta::CompilerMetaData::CmpGnTypeArgs) const &,
      decltype(meta::CompilerMetaData::CmpGnCompArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCmpFuncSig_<false, false, Ret, Args...> {
    using Type = Ret(*)(
      std::remove_reference_t<Args> &...);
  };

  template <bool HasGnTypeArgs, bool HasGnCompArgs, typename Ret, typename... Args>
  using DetermineCmpFuncSig = DetermineCmpFuncSig_<HasGnTypeArgs, HasGnCompArgs, Ret, Args...>::Type;
}

namespace spp::analyse::utils::cmp_utils {
  SPP_EXP_CLS struct CmpFn {
    decltype(meta::CompilerMetaData::CmpGnTypeArgs) GnTypeArgs;
    decltype(meta::CompilerMetaData::CmpGnCompArgs) GnCompArgs;
    ScopeManager *Sm;

    virtual ~CmpFn() = default;

    virtual auto invoke(
      Vec<Unique<ExpressionAst>> const &args)
      -> Unique<ExpressionAst> = 0;

    auto preload_generics(
      ScopeManager *sm,
      decltype(meta::CompilerMetaData::CmpGnTypeArgs) gn_type_args,
      decltype(meta::CompilerMetaData::CmpGnCompArgs) gn_comp_args)
      -> CmpFn& {
      Sm = sm;
      GnTypeArgs = std::move(gn_type_args);
      GnCompArgs = std::move(gn_comp_args);
      return *this;
    }
  };

  SPP_EXP_CLS template <bool HasGnTypeArgs, bool HasGnCompArgs, typename Ret, typename... Args>
  struct CmpFnImpl final : CmpFn {
    using FnPtr = DetermineCmpFuncSig<HasGnTypeArgs, HasGnCompArgs, Ret, Args...>;
    FnPtr fn;

    explicit CmpFnImpl(FnPtr f) : fn(std::move(f)) {
    }

    auto invoke(
      Vec<Unique<ExpressionAst>> const &args)
      -> Unique<ExpressionAst> override {
      return _InvokeImpl(args, std::index_sequence_for<Args...>{});
    }

  private:
    template <std::size_t... I>
    auto _InvokeImpl(
      Vec<Unique<ExpressionAst>> const &args,
      std::index_sequence<I...>)
      -> Unique<ExpressionAst> {
      if constexpr (HasGnTypeArgs and HasGnCompArgs) {
        INVOKE_PATTERN(*Sm, GnTypeArgs, GnCompArgs);
      }
      else if constexpr (HasGnTypeArgs) {
        INVOKE_PATTERN(*Sm, GnTypeArgs);
      }
      else if constexpr (HasGnCompArgs) {
        INVOKE_PATTERN(*Sm, GnCompArgs);
      }
      else {
        INVOKE_PATTERN()
      }
    }
  };

  /// Fold a comp-time value to a literal where that needs no
  /// analysis: a literal (spelled canonically), parentheses, a
  /// comp generic bound to such a value, and integer arithmetic,
  /// bit operations and comparisons over them, through the same
  /// comp-time intrinsics a "cmp" function runs. Null when the
  /// value is not closed (it names an unbound generic) or is not
  /// one of these shapes.
  SPP_EXP_FUN auto FoldCompExpr(ExpressionAst const &expr, Scope const &scope) -> Unique<ExpressionAst>;

  /// Stamp every comp generic named in a comp-time value -
  /// through parentheses and the operands of an operation, not
  /// only a bare name - with the parameter it names where the
  /// value is written, so a copy carried into another scope
  /// keeps naming it ("Scope::CanonVar").
  SPP_EXP_FUN auto StampCompGenerics(ExpressionAst const &expr, Scope const &scope) -> void;

  /// Append the identity of a comp-time value, read from "scope",
  /// to "out": a closed value is the literal it folds to, a comp
  /// generic is its parameter, parentheses are looked through,
  /// and an operation over them is written fully bracketed - so
  /// "(n + 1)" and "n + 1" are one value, "(a + b) * c" and
  /// "a + (b * c)" are not, and two scopes binding "n" apart
  /// are two values. Anything else is its spelling. Appends, so
  /// an operation's operands write into the one buffer and a
  /// caller keying many arguments reuses it.
  /// Todo: this will change from string to identity key soon.
  SPP_EXP_FUN auto CompExprIdentity(ExpressionAst const &expr, Scope const &scope, Str &out) -> void;

  SPP_EXP_FUN auto SetCompTimeAttrValue(
    ObjectInitializerAst const *object, Ast const *attribute, Unique<ExpressionAst> &&value,
    ScopeManager const *sm) -> void;

  SPP_EXP_FUN auto GetCompTimeAttrValue(
    ObjectInitializerAst const *object, IdentifierAst const *attribute) -> Unique<ExpressionAst>;

  SPP_EXP_FUN template <bool HasGnTypeArgs = false, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (not HasGnTypeArgs and not HasGnCompArgs)
  auto make_cmp_fn(Ret (*fn)(Args...)) -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<false, false, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (HasGnTypeArgs and not HasGnCompArgs)
  auto make_cmp_fn(
    Ret (*fn)(
      ScopeManager const &,
      decltype(meta::CompilerMetaData::CmpGnTypeArgs) const &,
      Args...))
    -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<true, false, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (not HasGnTypeArgs and HasGnCompArgs)
  auto make_cmp_fn(
    Ret (*fn)(
      ScopeManager const &,
      decltype(meta::CompilerMetaData::CmpGnCompArgs) const &,
      Args...))
    -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<false, true, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (HasGnTypeArgs and HasGnCompArgs)
  auto make_cmp_fn(
    Ret (*fn)(
      ScopeManager const &,
      decltype(meta::CompilerMetaData::CmpGnTypeArgs) const &,
      decltype(meta::CompilerMetaData::CmpGnCompArgs) const &,
      Args...))
    -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<true, true, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN auto std_intrinsics_add(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_add_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_sub(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_sub_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_mul(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_mul_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_div(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_div_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_rem(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_rem_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_sneg(
    IntegerLiteralAst const &val)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_shl(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_shl_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_shr(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_shr_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_ior(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_ior_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_and(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_and_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_xor(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_xor_assign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_not(
    IntegerLiteralAst const &val)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_not_assign(
    IntegerLiteralAst &lhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_abs(
    IntegerLiteralAst const &val)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_eq(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_oeq(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ne(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_one(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_lt(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_olt(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_le(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ole(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_gt(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ogt(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ge(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_oge(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_max_val(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_min_val(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_max(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_min(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_cmp(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fadd(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fadd_assign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fsub(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fsub_assign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fmul(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmul_assign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fdiv(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fdiv_assign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_frem(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_frem_assign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fneg(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fabs(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmax_val(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmin_val(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmax(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmin(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ffloor(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fceil(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ftrunc(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fround(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_float_neg_one()
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_float_zero()
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_float_one()
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_int_neg_one()
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_num_int_zero()
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_num_int_one()
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_num_int_two()
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_mem_ops_size_of(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto std_mem_ops_align_of(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;
}
