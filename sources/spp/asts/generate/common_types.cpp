#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_unary_expression_ast.hpp>
#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>

#include <genex/views/enumerate.hpp>


auto spp::asts::generate::common_types::f8(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("F8"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::f16(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("F16"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::f32(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("F32"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::f64(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("F64"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::f128(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("F128"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::s8(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("S8"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::s16(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("S16"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::s32(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("S32"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::s64(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("S64"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::s128(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("S128"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::s256(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("S256"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::ssize(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("SSize"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::u8(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("U8"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::u16(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("U16"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::u32(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("U32"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::u64(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("U64"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::u128(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("U128"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::u256(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("U256"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::usize(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("USize"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("number")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::void_type(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Void"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("void")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::boolean_type(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Bool"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("boolean")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::string_type(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Str"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("string")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::never_type(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Never"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("never")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::copy_type(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Copy"), nullptr);
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("copy")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::array_type(std::size_t pos, std::shared_ptr<TypeAst> elem_type, std::unique_ptr<ExpressionAst> &&size) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    generics_lst[1] = std::make_unique<GenericArgumentCompPositionalAst>(std::move(size));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Arr"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("array")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::variant_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(inner_types.size());
    for (auto &&[i, inner_type] : inner_types | genex::views::enumerate) {
        generics_lst[i] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    }
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Var"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("variant")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::tuple_type(std::size_t pos, std::vector<std::shared_ptr<TypeAst>> &&inner_types) -> std::shared_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(inner_types.size());
    for (auto &&[i, inner_type] : inner_types | genex::views::enumerate) {
        generics_lst[i] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    }
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::shared_ptr<TypeAst> type = std::make_shared<TypeIdentifierAst>(pos, std::string("Tup"), std::move(generics));
    type = std::make_shared<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("tuple")), nullptr), std::move(type));
    type = std::make_shared<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::future_type(std::size_t pos, std::shared_ptr<TypeAst> inner_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(1);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Fut"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("future")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::gen_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    generics_lst[1] = std::make_unique<GenericArgumentTypePositionalAst>(send_type ? std::move(send_type) : void_type(pos));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Gen"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("generator")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::gen_once_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(1);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("GenOnce"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("generator")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::gen_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> send_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    generics_lst[1] = std::make_unique<GenericArgumentTypePositionalAst>(send_type ? std::move(send_type) : void_type(pos));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("GenOpt"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("generator")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::gen_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type, std::shared_ptr<TypeAst> send_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(3);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    generics_lst[1] = std::make_unique<GenericArgumentTypePositionalAst>(err_type ? std::move(err_type) : void_type(pos));
    generics_lst[2] = std::make_unique<GenericArgumentTypePositionalAst>(send_type ? std::move(send_type) : void_type(pos));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("GenRes"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("generator")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::generated_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(1);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Generated"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("generator")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::generated_opt_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(1);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("GeneratedOpt"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("generator")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::generated_res_type(std::size_t pos, std::shared_ptr<TypeAst> yield_type, std::shared_ptr<TypeAst> err_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    generics_lst[1] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(err_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("GeneratedRes"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("generator")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::fun_ref_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("FunRef"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("function")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::fun_mut_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("FunMut"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("function")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::fun_mov_type(std::size_t pos, std::shared_ptr<TypeAst> param_types, std::shared_ptr<TypeAst> ret_type) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("FunMov"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("function")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::self_type(std::size_t pos) -> std::unique_ptr<TypeAst> {
    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Self"), nullptr);
    return type;
}
