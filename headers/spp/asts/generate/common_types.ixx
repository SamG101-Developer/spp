module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::generate::common_types {
    /**
     * Generate a TypeAst for the std:number::F8 type.
     * @param pos The position of the type in the source code.
     * @return The generated TypeAst.
     */
    SPP_EXP_FUN auto f8(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto f16(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto f32(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto f64(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto f128(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto s8(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto s16(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto s32(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto s64(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto s128(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto s256(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto ssize(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto u8(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto u16(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto u32(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto u64(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto u128(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto u256(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto usize(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto void_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto boolean_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto string_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto string_view_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto never_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto copy_type(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto array_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type, std::unique_ptr<ExpressionAst> &&size) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto variant_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto tuple_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto try_type(std::size_t pos, std::shared_ptr<TypeAst> output_type, std::shared_ptr<TypeAst> residual_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto future_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto option_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto memory_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto single_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto some_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto none_type(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto gen_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto gen_once_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto gen_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto gen_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto generated_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto generated_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto generated_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto index_mut_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto index_ref_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto fun_ref_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto fun_mut_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP_FUN auto fun_mov_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto self_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
}
