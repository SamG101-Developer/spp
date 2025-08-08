#ifndef COMMON_TYPES_HPP
#define COMMON_TYPES_HPP

#include <spp/asts/_fwd.hpp>


namespace spp::asts::generate::common_types {
    auto array_type(std::size_t pos, std::unique_ptr<TypeAst> elem_type, std::unique_ptr<ExpressionAst> size) -> std::unique_ptr<TypeAst>;
    auto variant_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst>;
    auto tuple_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst>;
}

#endif //COMMON_TYPES_HPP
