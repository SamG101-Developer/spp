module;
#include <spp/macros.hpp>

#define INVOKE_PATTERN(...)                                          \
  if constexpr (std::is_same_v<Ret, void>) {                         \
    fn(__VA_OPT__(__VA_ARGS__, ) ArgAs<Args>(*args[I])...);          \
    return nullptr;                                                  \
  }                                                                  \
  else {                                                             \
    auto v = fn(__VA_OPT__(__VA_ARGS__, ) ArgAs<Args>(*args[I])...); \
    return v;                                                        \
  }

export module spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.scopes.comp_key;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct Ast);
use(spp::asts, struct BooleanLiteralAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FloatLiteralAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct IntegerLiteralAst);
use(spp::asts, struct ObjectInitializerAst);

namespace spp {
  /// An argument as the intrinsic's parameter type. Analysis
  /// has checked the call, so any other kind is an internal
  /// compiler error ("As").
  template <typename Param, typename Arg>
  auto ArgAs(Arg &arg) -> std::remove_reference_t<Param>& {
    return *arg.template As<std::remove_cvref_t<Param>>();
  }

  template <bool HasGnTypeArgs, bool HasGnCompArgs, typename Ret, typename... Args>
  struct DetermineCompTimeFnSig_ {
    using Type = void;
  };

  template <typename Ret, typename... Args>
  struct DetermineCompTimeFnSig_<true, false, Ret, Args...> {
    using Type = Ret(*)(
      ScopeManager const &,
      decltype(meta::CompTimeCallArgs::GnTypeArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCompTimeFnSig_<false, true, Ret, Args...> {
    using Type = Ret(*)(
      ScopeManager const &,
      decltype(meta::CompTimeCallArgs::GnCompArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCompTimeFnSig_<true, true, Ret, Args...> {
    using Type = Ret(*)(
      ScopeManager const &,
      decltype(meta::CompTimeCallArgs::GnTypeArgs) const &,
      decltype(meta::CompTimeCallArgs::GnCompArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCompTimeFnSig_<false, false, Ret, Args...> {
    using Type = Ret(*)(
      std::remove_reference_t<Args> &...);
  };

  template <bool HasGnTypeArgs, bool HasGnCompArgs, typename Ret, typename... Args>
  using DetermineCompTimeFnSig = DetermineCompTimeFnSig_<HasGnTypeArgs, HasGnCompArgs, Ret, Args...>::Type;
}

namespace spp::analyse::utils::comp_time_intrinsics {
  SPP_EXP_CLS struct CompTimeFn {
    decltype(meta::CompTimeCallArgs::GnTypeArgs) GnTypeArgs;
    decltype(meta::CompTimeCallArgs::GnCompArgs) GnCompArgs;
    ScopeManager *Sm;

    virtual ~CompTimeFn() = default;

    virtual auto Invoke(
      Vec<Unique<ExpressionAst>> const &args)
      -> Unique<ExpressionAst> = 0;

    auto PreloadGns(
      ScopeManager *sm,
      decltype(meta::CompTimeCallArgs::GnTypeArgs) gn_type_args,
      decltype(meta::CompTimeCallArgs::GnCompArgs) gn_comp_args)
      -> CompTimeFn& {
      Sm = sm;
      GnTypeArgs = std::move(gn_type_args);
      GnCompArgs = std::move(gn_comp_args);
      return *this;
    }
  };

  SPP_EXP_CLS template <bool HasGnTypeArgs, bool HasGnCompArgs, typename Ret, typename... Args>
  struct CompTimeFnImpl final : CompTimeFn {
    using FnPtr = DetermineCompTimeFnSig<HasGnTypeArgs, HasGnCompArgs, Ret, Args...>;
    FnPtr fn;

    explicit CompTimeFnImpl(FnPtr f) : fn(std::move(f)) {
    }

    auto Invoke(
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

  SPP_EXP_FUN auto SetCompTimeAttrValue(
    ObjectInitializerAst const *object, Ast const *attribute, Unique<ExpressionAst> &&value,
    ScopeManager const *sm) -> void;

  SPP_EXP_FUN auto GetCompTimeAttrValue(
    ObjectInitializerAst const *object, IdentifierAst const *attribute) -> Unique<ExpressionAst>;

  SPP_EXP_FUN template <bool HasGnTypeArgs = false, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (not HasGnTypeArgs and not HasGnCompArgs)
  auto MakeCompTimeFn(Ret (*fn)(Args...)) -> Unique<CompTimeFn> {
    return MakeUnique<CompTimeFnImpl<false, false, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (HasGnTypeArgs and not HasGnCompArgs)
  auto MakeCompTimeFn(
    Ret (*fn)(
      ScopeManager const &,
      decltype(meta::CompTimeCallArgs::GnTypeArgs) const &,
      Args...))
    -> Unique<CompTimeFn> {
    return MakeUnique<CompTimeFnImpl<true, false, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (not HasGnTypeArgs and HasGnCompArgs)
  auto MakeCompTimeFn(
    Ret (*fn)(
      ScopeManager const &,
      decltype(meta::CompTimeCallArgs::GnCompArgs) const &,
      Args...))
    -> Unique<CompTimeFn> {
    return MakeUnique<CompTimeFnImpl<false, true, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (HasGnTypeArgs and HasGnCompArgs)
  auto MakeCompTimeFn(
    Ret (*fn)(
      ScopeManager const &,
      decltype(meta::CompTimeCallArgs::GnTypeArgs) const &,
      decltype(meta::CompTimeCallArgs::GnCompArgs) const &,
      Args...))
    -> Unique<CompTimeFn> {
    return MakeUnique<CompTimeFnImpl<true, true, Ret, Args...>>(fn);
  }

  /// Whether a divisor is zero: an integer or float literal, as "/" and "%" cannot fold over. The call raises it
  /// (E90); a key leaves the operation unfolded.
  SPP_EXP_FUN auto IsZeroDivisor(
    ExpressionAst const &divisor)
    -> bool;

  /// The width in bits of an integer type, by its suffix ("s32" is 32); the pointer-sized ones are as wide as a pointer
  /// here.
  SPP_EXP_FUN auto IntegerWidth(
    StrView type)
    -> std::int64_t;

  /// Whether a shift amount is as wide as the type it shifts, or wider: a shift cannot fold over one. The call raises
  /// it (E91); a key leaves the operation unfolded.
  SPP_EXP_FUN auto IsShiftTooWide(
    IntegerLiteralAst const &value,
    IntegerLiteralAst const &amount)
    -> bool;

  /// A literal's value key, which "A[1 + 2]" and "A[3]" both key as: one value however it is spelled ("0x10_uz" and
  /// "16_uz"), a float as its value in its type's format. None for a float its type cannot hold (an infinity, a NaN),
  /// or anything but a literal.
  SPP_EXP_FUN auto ValueKeyOf(
    ExpressionAst const &lit)
    -> Opt<scopes::CompKey>;

  /// A value key as the literal it is; null for any other part.
  SPP_EXP_FUN auto LiteralOf(
    scopes::CompKey const &key)
    -> Unique<ExpressionAst>;

  /// Fold a binary operator over two literal operands as its std method does: "+" over integers is "add", whose body
  /// calls "std::intrinsics::sadd" or "uadd", which fold by "StdIntrinsicsAdd". "and" and "or" are the language's own,
  /// not methods. Null when the operator has no intrinsic over those operands, or the intrinsic cannot fold them (a
  /// zero divisor, a shift too wide), so a comp key and a folded call agree on every value.
  SPP_EXP_FUN auto FoldBinaryOp(
    StrView op,
    ExpressionAst const &lhs,
    ExpressionAst const &rhs)
    -> Unique<ExpressionAst>;

  SPP_EXP_FUN auto StdIntrinsicsAdd(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsAddAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsSub(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsSubAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsMul(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsMulAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsDiv(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsDivAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsRem(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsRemAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsSneg(
    IntegerLiteralAst const &val)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsBitShl(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsBitShlAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsBitShr(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsBitShrAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsBitIor(
    ExpressionAst const &lhs,
    ExpressionAst const &rhs)
    -> Unique<ExpressionAst>;

  SPP_EXP_FUN auto StdIntrinsicsBitIorAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsBitAnd(
    ExpressionAst const &lhs,
    ExpressionAst const &rhs)
    -> Unique<ExpressionAst>;

  SPP_EXP_FUN auto StdIntrinsicsBitAndAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsBitXor(
    ExpressionAst const &lhs,
    ExpressionAst const &rhs)
    -> Unique<ExpressionAst>;

  SPP_EXP_FUN auto StdIntrinsicsBitXorAssign(
    IntegerLiteralAst &lhs,
    IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsBitNot(
    ExpressionAst const &val)
    -> Unique<ExpressionAst>;

  SPP_EXP_FUN auto StdIntrinsicsBitNotAssign(
    IntegerLiteralAst &lhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsEq(
    ExpressionAst const &lhs,
    ExpressionAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsOeq(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsNe(
    ExpressionAst const &lhs,
    ExpressionAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsOne(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsLt(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsOlt(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsLe(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsOle(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsGt(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsOgt(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsGe(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsOge(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<BooleanLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsMaxVal(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsMinVal(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsMax(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsMin(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsCmp(
    IntegerLiteralAst const &lhs,
    IntegerLiteralAst const &rhs)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFadd(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFaddAssign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsFsub(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFsubAssign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsFmul(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFmulAssign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsFdiv(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFdivAssign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsFrem(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFremAssign(
    FloatLiteralAst &lhs,
    FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto StdIntrinsicsFneg(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFabs(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFmaxVal(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFminVal(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFmax(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFmin(
    FloatLiteralAst const &lhs,
    FloatLiteralAst const &rhs)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFfloor(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFceil(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFtrunc(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdIntrinsicsFround(
    FloatLiteralAst const &val)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdNumFloatNegOne(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdNumFloatZero(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdNumFloatOne(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<FloatLiteralAst>;

  SPP_EXP_FUN auto StdNumIntNegOne(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdNumIntZero(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdNumIntOne(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdNumIntTwo(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdMemOpsSizeOf(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;

  SPP_EXP_FUN auto StdMemOpsAlignOf(
    ScopeManager const &sm,
    Vec<Shared<TypeRef>> const &types)
    -> Unique<IntegerLiteralAst>;
}
