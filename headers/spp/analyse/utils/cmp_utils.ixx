module;
#include <spp/macros.hpp>
#include <opex/macros.hpp>

export module spp.analyse.utils.cmp_utils;
import opex.ops;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct FloatLiteralAst;
    SPP_EXP_CLS struct IntegerLiteralAst;
}


OPEX_MAKE_UNARY_OP(cmp_abs, std::int64_t) {
    OPEX_PROLOGUE;
    return std::abs(rhs);
}

OPEX_MAKE_BIN_OP(cmp_smax, std::int64_t, std::int64_t) {
    OPEX_PROLOGUE;
    return std::max(lhs, rhs);
}

OPEX_MAKE_BIN_OP(cmp_umax, std::uint64_t, std::uint64_t) {
    OPEX_PROLOGUE;
    return std::max(lhs, rhs);
}

OPEX_MAKE_BIN_OP(cmp_smin, std::int64_t, std::int64_t) {
    OPEX_PROLOGUE;
    return std::min(lhs, rhs);
}

OPEX_MAKE_BIN_OP(cmp_umin, std::uint64_t, std::uint64_t) {
    OPEX_PROLOGUE;
    return std::min(lhs, rhs);
}

OPEX_MAKE_UNARY_OP(cmp_fabs, double) {
    OPEX_PROLOGUE;
    return std::fabs(rhs);
}

OPEX_MAKE_BIN_OP(cmp_fmax, double, double) {
    OPEX_PROLOGUE;
    return std::fmax(lhs, rhs);
}

OPEX_MAKE_BIN_OP(cmp_fmin, double, double) {
    OPEX_PROLOGUE;
    return std::fmin(lhs, rhs);
}

OPEX_MAKE_UNARY_OP(cmp_ffloor, double) {
    OPEX_PROLOGUE;
    return std::floor(rhs);
}

OPEX_MAKE_UNARY_OP(cmp_fceil, double) {
    OPEX_PROLOGUE;
    return std::ceil(rhs);
}

OPEX_MAKE_UNARY_OP(cmp_ftrunc, double) {
    OPEX_PROLOGUE;
    return std::trunc(rhs);
}

OPEX_MAKE_UNARY_OP(cmp_fround, double) {
    OPEX_PROLOGUE;
    return std::round(rhs);
}

OPEX_MAKE_BIN_OP(cmp_frem, double, double) {
    OPEX_PROLOGUE;
    return std::remainder(lhs, rhs);
}


namespace spp::analyse::utils::cmp_utils {
    SPP_EXP_CLS struct CmpFn {
        virtual ~CmpFn() = default;
        virtual auto invoke(
            std::vector<std::unique_ptr<asts::ExpressionAst>> const &args)
            -> std::unique_ptr<asts::ExpressionAst> = 0;
    };

    SPP_EXP_CLS template <typename Ret, typename... Args>
    struct CmpFnImpl : CmpFn {
        using FnPtr = Ret(*)(Args...);
        FnPtr fn;

        explicit CmpFnImpl(FnPtr f) : fn(std::move(f)) {
        }

        auto invoke(
            std::vector<std::unique_ptr<asts::ExpressionAst>> const &args)
            -> std::unique_ptr<asts::ExpressionAst> override {
            return invoke_impl(args, std::index_sequence_for<Args...>{});
        }

    private:
        template <std::size_t... I>
        auto invoke_impl(
            std::vector<std::unique_ptr<asts::ExpressionAst>> const &args,
            std::index_sequence<I...>)
            -> std::unique_ptr<asts::ExpressionAst> {
            if constexpr (std::is_same_v<Ret, void>) {
                fn(dynamic_cast<std::remove_reference_t<Args>&>(*args[I])...);
                return nullptr;
            }
            else {
                return fn(dynamic_cast<std::remove_reference_t<Args>&>(*args[I])...);
            }
        }
    };

    SPP_EXP_FUN template <typename Ret, typename... Args>
    auto make_cmp_fn(Ret (*fn)(Args...)) -> std::unique_ptr<CmpFn> {
        return std::make_unique<CmpFnImpl<Ret, Args...>>(fn);
    }

    SPP_EXP_FUN auto std_boolean_bit_and(
        asts::BooleanLiteralAst const &lhs,
        asts::BooleanLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_boolean_bit_ior(
        asts::BooleanLiteralAst const &lhs,
        asts::BooleanLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_boolean_bit_xor(
        asts::BooleanLiteralAst const &lhs,
        asts::BooleanLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_boolean_bit_not(
        asts::BooleanLiteralAst const &val)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_boolean_and(
        asts::BooleanLiteralAst const &lhs,
        asts::BooleanLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_boolean_ior(
        asts::BooleanLiteralAst const &lhs,
        asts::BooleanLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_boolean_eq(
        asts::BooleanLiteralAst const &lhs,
        asts::BooleanLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_boolean_ne(
        asts::BooleanLiteralAst const &lhs,
        asts::BooleanLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_add(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_add_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_sub(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_sub_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_mul(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_mul_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_sdiv(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_sdiv_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_udiv(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_udiv_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_srem(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_srem_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_urem(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_urem_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_sneg(
        asts::IntegerLiteralAst const &val)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_bit_shl(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_bit_shl_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_bit_shr(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_bit_shr_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_bit_ior(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_bit_ior_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_bit_and(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_bit_and_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_bit_xor(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_bit_xor_assign(
        asts::IntegerLiteralAst &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_bit_not(
        asts::IntegerLiteralAst const &val)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_bit_not_assign(
        asts::IntegerLiteralAst &lhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_abs(
        asts::IntegerLiteralAst const &val)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_eq(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_oeq(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ne(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_one(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_slt(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ult(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_olt(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_sle(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ule(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ole(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_sgt(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ugt(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ogt(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_sge(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_uge(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_oge(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::BooleanLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_smax(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_umax(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_smin(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_umin(
        asts::IntegerLiteralAst const &lhs,
        asts::IntegerLiteralAst const &rhs)
        -> std::unique_ptr<asts::IntegerLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fadd(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fadd_assign(
        asts::FloatLiteralAst &lhs,
        asts::FloatLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_fsub(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fsub_assign(
        asts::FloatLiteralAst &lhs,
        asts::FloatLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_fmul(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fmul_assign(
        asts::FloatLiteralAst &lhs,
        asts::FloatLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_fdiv(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fdiv_assign(
        asts::FloatLiteralAst &lhs,
        asts::FloatLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_frem(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_frem_assign(
        asts::FloatLiteralAst &lhs,
        asts::FloatLiteralAst const &rhs)
        -> void;

    SPP_EXP_FUN auto std_intrinsics_fneg(
        asts::FloatLiteralAst const &val)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fabs(
        asts::FloatLiteralAst const &val)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fmax(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fmin(
        asts::FloatLiteralAst const &lhs,
        asts::FloatLiteralAst const &rhs)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ffloor(
        asts::FloatLiteralAst const &val)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fceil(
        asts::FloatLiteralAst const &val)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_ftrunc(
        asts::FloatLiteralAst const &val)
        -> std::unique_ptr<asts::FloatLiteralAst>;

    SPP_EXP_FUN auto std_intrinsics_fround(
        asts::FloatLiteralAst const &val)
        -> std::unique_ptr<asts::FloatLiteralAst>;
}
