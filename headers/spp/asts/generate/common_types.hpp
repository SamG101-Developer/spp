#ifndef COMMON_TYPES_HPP
#define COMMON_TYPES_HPP

#include <spp/asts/_fwd.hpp>


namespace spp::asts::generate::common_types {
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
    auto array_type(std::size_t pos, std::unique_ptr<TypeAst> elem_type, std::unique_ptr<ExpressionAst> size) -> std::unique_ptr<TypeAst>;
    auto variant_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst>;
    auto tuple_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst>;
    auto fun_ref_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&param_types, std::unique_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst>;
    auto fun_mut_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&param_types, std::unique_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst>;
    auto fun_mov_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&param_types, std::unique_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst>;

    auto self_type(std::size_t pos) -> std::unique_ptr<TypeAst>;

    struct Precompiled {
        static std::unique_ptr<TypeAst> VOID;
        static std::unique_ptr<TypeAst> BOOL;

        auto initialize() -> void;
    };
}

#endif //COMMON_TYPES_HPP
