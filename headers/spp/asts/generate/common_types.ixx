module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types;
import spp.asts._fwd;

import std;


namespace spp::asts::generate::common_types {
    /**
     * Generate a TypeAst for the std:number::F8 type.
     * @param pos The position of the type in the source code.
     * @return The generated TypeAst.
     */
    SPP_EXP auto f8(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto f16(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto f32(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto f64(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto f128(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto s8(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto s16(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto s32(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto s64(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto s128(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto s256(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto ssize(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto u8(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto u16(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto u32(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto u64(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto u128(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto u256(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto usize(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto void_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto boolean_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto string_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto never_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto copy_type(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto array_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type, std::unique_ptr<ExpressionAst> &&size) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto variant_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto tuple_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto try_type(std::size_t pos, std::shared_ptr<TypeAst> output_type, std::shared_ptr<TypeAst> residual_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto future_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto option_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto memory_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto single_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto some_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto none_type(std::size_t pos) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto gen_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto gen_once_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto gen_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto gen_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto generated_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto generated_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto generated_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto index_mut_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto index_ref_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto fun_ref_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto fun_mut_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::shared_ptr<TypeAst>;
    SPP_EXP auto fun_mov_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::shared_ptr<TypeAst>;

    SPP_EXP auto self_type(std::size_t pos) -> std::shared_ptr<TypeAst>;
}
