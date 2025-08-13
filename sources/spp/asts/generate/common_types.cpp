#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_unary_expression_ast.hpp>
#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>


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


auto spp::asts::generate::common_types::array_type(std::size_t pos, std::unique_ptr<TypeAst> elem_type, std::unique_ptr<ExpressionAst> size) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(2);
    generics_lst[0] = std::make_unique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    generics_lst[1] = std::make_unique<GenericArgumentCompPositionalAst>(std::move(size));
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Arr"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("array")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::variant_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(inner_types.size());
    for (auto &&inner_type : inner_types) {
        generics_lst.push_back(std::make_unique<GenericArgumentTypePositionalAst>(std::move(inner_type)));
    }
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Var"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("variant")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}


auto spp::asts::generate::common_types::tuple_type(std::size_t pos, std::vector<std::unique_ptr<TypeAst>> &&inner_types) -> std::unique_ptr<TypeAst> {
    auto generics_lst = std::vector<std::unique_ptr<GenericArgumentAst>>(inner_types.size());
    for (auto &&inner_type : inner_types) {
        generics_lst.push_back(std::make_unique<GenericArgumentTypePositionalAst>(std::move(inner_type)));
    }
    auto generics = std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    std::unique_ptr<TypeAst> type = std::make_unique<TypeIdentifierAst>(pos, std::string("Tup"), std::move(generics));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("tuple")), nullptr), std::move(type));
    type = std::make_unique<TypeUnaryExpressionAst>(std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(std::make_unique<IdentifierAst>(pos, std::string("std")), nullptr), std::move(type));
    return type;
}
