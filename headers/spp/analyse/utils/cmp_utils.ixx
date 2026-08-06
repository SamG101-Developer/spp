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

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct TypeSymbol;
  SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct BooleanLiteralAst;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct FloatLiteralAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct IntegerLiteralAst;
  SPP_EXP_CLS struct ObjectInitializerAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp {
  template <bool HasGnTypeArgs, bool HasGnCompArgs, typename Ret, typename... Args>
  struct DetermineCmpFuncSig_ {
    using Type = void;
  };

  template <typename Ret, typename... Args>
  struct DetermineCmpFuncSig_<true, false, Ret, Args...> {
    using Type = Ret(*)(
      spp::analyse::scopes::ScopeManager const &,
      decltype(spp::asts::meta::CompilerMetaData::CmpGnTypeArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCmpFuncSig_<false, true, Ret, Args...> {
    using Type = Ret(*)(
      spp::analyse::scopes::ScopeManager const &,
      decltype(spp::asts::meta::CompilerMetaData::CmpGnCompArgs) const &,
      std::remove_reference_t<Args> &...);
  };

  template <typename Ret, typename... Args>
  struct DetermineCmpFuncSig_<true, true, Ret, Args...> {
    using Type = Ret(*)(
      spp::analyse::scopes::ScopeManager const &,
      decltype(spp::asts::meta::CompilerMetaData::CmpGnTypeArgs) const &,
      decltype(spp::asts::meta::CompilerMetaData::CmpGnCompArgs) const &,
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
    decltype(asts::meta::CompilerMetaData::CmpGnTypeArgs) GnTypeArgs;
    decltype(asts::meta::CompilerMetaData::CmpGnCompArgs) GnCompArgs;
    scopes::ScopeManager *ScopeManager;

    virtual ~CmpFn() = default;

    virtual auto invoke(
      Vec<Unique<asts::ExpressionAst>> const &args)
      -> Unique<asts::ExpressionAst> = 0;

    auto preload_generics(
      scopes::ScopeManager *sm,
      decltype(asts::meta::CompilerMetaData::CmpGnTypeArgs) gn_type_args,
      decltype(asts::meta::CompilerMetaData::CmpGnCompArgs) gn_comp_args)
      -> CmpFn& {
      ScopeManager = sm;
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
      Vec<Unique<asts::ExpressionAst>> const &args)
      -> Unique<asts::ExpressionAst> override {
      return invoke_impl(args, std::index_sequence_for<Args...>{});
    }

  private:
    template <std::size_t... I>
    auto invoke_impl(
      Vec<Unique<asts::ExpressionAst>> const &args,
      std::index_sequence<I...>)
      -> Unique<asts::ExpressionAst> {
      if constexpr (HasGnTypeArgs and HasGnCompArgs) {
        INVOKE_PATTERN(*ScopeManager, GnTypeArgs, GnCompArgs);
      }
      else if constexpr (HasGnTypeArgs) {
        INVOKE_PATTERN(*ScopeManager, GnTypeArgs);
      }
      else if constexpr (HasGnCompArgs) {
        INVOKE_PATTERN(*ScopeManager, GnCompArgs);
      }
      else {
        INVOKE_PATTERN()
      }
    }
  };

  SPP_EXP_FUN auto SetCompTimeAttrValue(
    asts::ObjectInitializerAst const *object,
    asts::Ast *attribute,
    Unique<asts::ExpressionAst> &&value,
    scopes::ScopeManager const *sm)
    -> void;

  SPP_EXP_FUN auto GetCompTimeAttrValue(
    asts::ObjectInitializerAst const *object,
    asts::IdentifierAst const *attribute)
    -> Unique<asts::ExpressionAst>;

  SPP_EXP_FUN template <bool HasGnTypeArgs = false, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (not HasGnTypeArgs and not HasGnCompArgs)
  auto make_cmp_fn(Ret (*fn)(Args...)) -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<false, false, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (HasGnTypeArgs and not HasGnCompArgs)
  auto make_cmp_fn(
    Ret (*fn)(
      scopes::ScopeManager const &,
      decltype(asts::meta::CompilerMetaData::CmpGnTypeArgs) const &,
      Args...))
    -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<true, false, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (not HasGnTypeArgs and HasGnCompArgs)
  auto make_cmp_fn(
    Ret (*fn)(
      scopes::ScopeManager const &,
      decltype(asts::meta::CompilerMetaData::CmpGnCompArgs) const &,
      Args...))
    -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<false, true, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN template <bool HasGnTypeArgs, bool HasGnCompArgs = false, typename Ret, typename... Args>
    requires (HasGnTypeArgs and HasGnCompArgs)
  auto make_cmp_fn(
    Ret (*fn)(
      scopes::ScopeManager const &,
      decltype(asts::meta::CompilerMetaData::CmpGnTypeArgs) const &,
      decltype(asts::meta::CompilerMetaData::CmpGnCompArgs) const &,
      Args...))
    -> Unique<CmpFn> {
    return MakeUnique<CmpFnImpl<true, true, Ret, Args...>>(fn);
  }

  SPP_EXP_FUN auto std_boolean_and(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_boolean_ior(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_add(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_add_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_sub(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_sub_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_mul(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_mul_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_sdiv(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_sdiv_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_udiv(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_udiv_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_srem(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_srem_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_urem(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_urem_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_sneg(
    asts::IntegerLiteralAst const &val)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_shl(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_shl_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_shr(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_shr_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_ior(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_ior_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_and(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_and_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_xor(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_xor_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_bit_not(
    asts::IntegerLiteralAst const &val)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_bit_not_assign(
    asts::IntegerLiteralAst &lhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_abs(
    asts::IntegerLiteralAst const &val)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_eq(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_oeq(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ne(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_one(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_slt(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ult(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_olt(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_sle(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ule(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ole(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_sgt(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ugt(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ogt(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_sge(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_uge(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_oge(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::BooleanLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_max_val(
    asts::IntegerLiteralAst const &val)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_min_val(
    asts::IntegerLiteralAst const &val)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_smax(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_umax(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_smin(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_umin(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_scmp(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ucmp(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fcmp(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fadd(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fadd_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fsub(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fsub_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fmul(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmul_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fdiv(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fdiv_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_frem(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_frem_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void;

  SPP_EXP_FUN auto std_intrinsics_fneg(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fabs(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmax_val(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmin_val(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmax(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fmin(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ffloor(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fceil(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_ftrunc(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_intrinsics_fround(
    asts::FloatLiteralAst const &val)
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_float_neg_one()
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_float_zero()
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_float_one()
    -> Unique<asts::FloatLiteralAst>;

  SPP_EXP_FUN auto std_num_int_neg_one()
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_num_int_zero()
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_num_int_one()
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_num_int_two()
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_mem_ops_size_of(
    scopes::ScopeManager const &sm,
    Vec<asts::TypeAst*> const &types)
    -> Unique<asts::IntegerLiteralAst>;

  SPP_EXP_FUN auto std_mem_ops_align_of(
    scopes::ScopeManager const &sm,
    Vec<asts::TypeAst*> const &types)
    -> Unique<asts::IntegerLiteralAst>;
}
