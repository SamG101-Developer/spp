#pragma once
#include <spp/asts/_fwd.hpp>


/// @cond
namespace spp::asts::generate::common_types {
    /**
     * Generate a TypeAst for the std:number::F8 type.
     * @param pos The position of the type in the source code.
     * @return The generated TypeAst.
     */
    auto f8(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto f16(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto f32(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto f64(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto f128(std::size_t pos) -> std::unique_ptr<TypeAst>;

    auto s8(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto s16(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto s32(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto s64(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto s128(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto s256(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto ssize(std::size_t pos) -> std::unique_ptr<TypeAst>;

    auto u8(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto u16(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto u32(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto u64(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto u128(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto u256(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto usize(std::size_t pos) -> std::unique_ptr<TypeAst>;

    auto void_type(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto boolean_type(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto string_type(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto never_type(std::size_t pos) -> std::unique_ptr<TypeAst>;
    auto copy_type(std::size_t pos) -> std::unique_ptr<TypeAst>;

    auto array_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type, std::unique_ptr<ExpressionAst> &&size) -> std::unique_ptr<TypeAst>;
    auto variant_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst>;
    auto tuple_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::shared_ptr<TypeAst>;
    auto future_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::unique_ptr<TypeAst>;

    auto gen_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::unique_ptr<TypeAst>;
    auto gen_once_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::unique_ptr<TypeAst>;
    auto gen_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::unique_ptr<TypeAst>;
    auto gen_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type, std::shared_ptr<TypeAst> send_type = nullptr) -> std::unique_ptr<TypeAst>;

    auto generated_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::unique_ptr<TypeAst>;
    auto generated_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::unique_ptr<TypeAst>;
    auto generated_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type) -> std::unique_ptr<TypeAst>;

    auto fun_ref_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst>;
    auto fun_mut_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst>;
    auto fun_mov_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst>;

    auto self_type(std::size_t pos) -> std::unique_ptr<TypeAst>;
}
/// @endcond
