module spp.asts.generate.common_types;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_type_positional_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.type_ast;
import genex;

auto spp::asts::generate::common_types::F8(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("F8"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::F16(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("F16"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::F32(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("F32"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::F64(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("F64"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::F128(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("F128"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::S8(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("S8"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::S16(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("S16"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::S32(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("S32"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::S64(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("S64"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::S128(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("S128"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::S256(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("S256"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SSize(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("SSize"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::U8(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("U8"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::U16(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("U16"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::U32(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("U32"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::U64(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("U64"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::U128(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("U128"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::U256(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("U256"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::USize(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("USize"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("number")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::CharType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Char"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("char")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::VoidType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Void"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("void")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::BooleanType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Bool"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("boolean")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::StringType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Str"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("string")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::StringViewType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("StrView"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("string_view")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));

    // As it is a view, we add the "&" convention by default.
    auto op = MakeShared<TypeUnaryExpressionOperatorBorrowAst>(MakeUnique<ConventionRefAst>(nullptr));
    type = MakeShared<TypeUnaryExpressionAst>(op, std::move(type));
    return type;
}

auto spp::asts::generate::common_types::NeverType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Never"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("never")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::CopyType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Copy"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("copy")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::ArrayType(std::size_t pos, Shared<TypeAst> elem_type, Unique<ExpressionAst> &&size) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    generics_lst[1] = MakeUnique<GenericArgumentCompPositionalAst>(std::move(size));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Arr"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("array")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::VecU8Type(const std::size_t pos) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(U8(pos));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Vec"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("vector")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::VariantType(std::size_t pos, SharedVec<TypeAst> &&inner_types) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(inner_types.Len());
    for (auto &&[i, inner_type] : inner_types | genex::views::enumerate) {
        generics_lst[i] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    }
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Var"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("variant")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::TupleType(std::size_t pos, SharedVec<TypeAst> &&inner_types) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(inner_types.Len());
    for (auto &&[i, inner_type] : inner_types | genex::views::enumerate) {
        generics_lst[i] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    }
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Tup"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("tuple")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::TryType(std::size_t pos, Shared<TypeAst> output_type, Shared<TypeAst> residual_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(output_type));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(residual_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Try"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("try")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::FutureType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Fut"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("future")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::OptionType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Opt"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("option")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::MemoryType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Memory"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("memory")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SingleType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Single"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("single")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::ViewType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("View"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("view")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SomeType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Some"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("option")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::None(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("None"), nullptr);
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("option")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::GenType(std::size_t pos, Shared<TypeAst> yield_type, Shared<TypeAst> send_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(send_type ? std::move(send_type) : VoidType(pos));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Gen"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("generator")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::GenOnceType(std::size_t pos, Shared<TypeAst> yield_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("GenOnce"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("generator")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::IndexMutType(std::size_t pos, Shared<TypeAst> elem_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("IndexMut"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::IndexRefType(std::size_t pos, Shared<TypeAst> elem_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("IndexRef"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SliceMutType(std::size_t pos, Shared<TypeAst> elem_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("SliceMut"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SliceRefType(std::size_t pos, Shared<TypeAst> elem_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("SliceRef"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::FunRefType(std::size_t pos, Shared<TypeAst> param_types, Shared<TypeAst> ret_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("FunRef"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("function")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::FunMutType(std::size_t pos, Shared<TypeAst> param_types, Shared<TypeAst> ret_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("FunMut"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("function")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::FunMovType(std::size_t pos, Shared<TypeAst> param_types, Shared<TypeAst> ret_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("FunMov"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("function")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::ForwardRefType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("FwdRef"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("fwd")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::ForwardMutType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("FwdMut"), std::move(generics));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("fwd")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeShared<TypeUnaryExpressionAst>(MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(MakeShared<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SelfType(std::size_t pos) -> Shared<TypeAst> {
    Shared<TypeAst> type = MakeShared<TypeIdentifierAst>(pos, Str("Self"), nullptr);
    return type;
}
