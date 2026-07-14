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

#define MAKE_TYPE(ty) \
    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str(ty), nullptr);

#define MAKE_TYPE_WITH_GN(ty) \
    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str(ty), std::move(generics));

#define ADD_NAMESPACE(ns) \
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str(ns)), nullptr), std::move(type));

#define ADD_BORROW() \
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(MakeUnique<ConventionRefAst>(nullptr)), std::move(type));

#define FINISH_TYPE() \
    return type;

#define MAKE_GENERICS() \
    auto generics_list = Vec<Unique<GenericArgumentAst>>();

#define ADD_TYPE_GENERIC(gn) \
    generics_list.EmplaceBack(MakeUnique<GenericArgumentTypePositionalAst>(std::move(gn)));

#define ADD_COMP_GENERIC(gn) \
    generics_list.EmplaceBack(MakeUnique<GenericArgumentCompPositionalAst>(std::move(gn)));

#define ADD_ALL_GENERICS()                                                                            \
    for (auto &&inner_type : inner_types) {                                                           \
        generics_list.EmplaceBack(MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type))); \
    }

#define FINISH_GENERICS() \
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_list), nullptr);

auto spp::asts::generate::common_types::F8(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("F8");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::F16(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("F16");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::F32(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("F32");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::F64(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("F64");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::F128(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("F128");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::S8(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("S8");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::S16(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("S16");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::S32(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("S32");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::S64(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("S64");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::S128(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("S128");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::S256(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("S256");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::SSize(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("SSize");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::U8(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("U8");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::U16(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("U16");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::U32(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("U32");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::U64(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("U64");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::U128(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("U128");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::U256(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("U256");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::USize(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("USize");
    ADD_NAMESPACE("number");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::CharType(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("Char");
    ADD_NAMESPACE("char");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::VoidType(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("Void");
    ADD_NAMESPACE("void");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::BooleanType(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("Bool");
    ADD_NAMESPACE("boolean");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::StringType(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("Str");
    ADD_NAMESPACE("string");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::StringViewType(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("StrView");
    ADD_NAMESPACE("string_view");
    ADD_NAMESPACE("std");
    ADD_BORROW()
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::NeverType(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("Never");
    ADD_NAMESPACE("never");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::CopyType(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("Copy");
    ADD_NAMESPACE("copy");
    ADD_NAMESPACE("std");
    FINISH_TYPE();
}

auto spp::asts::generate::common_types::ArrayType(std::size_t pos, Unique<TypeAst> elem_type, Unique<ExpressionAst> &&size) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(elem_type)
    ADD_COMP_GENERIC(size)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Arr")
    ADD_NAMESPACE("array")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::ViewU8Type(const std::size_t pos) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(U8(pos))
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("View")
    ADD_NAMESPACE("view")
    ADD_NAMESPACE("std")
    ADD_BORROW()
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::VariantType(std::size_t pos, UniqueVec<TypeAst> &&inner_types) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_ALL_GENERICS()
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Var")
    ADD_NAMESPACE("variant")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::TupleType(std::size_t pos, UniqueVec<TypeAst> &&inner_types) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_ALL_GENERICS()
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Tup")
    ADD_NAMESPACE("tuple")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::TryType(std::size_t pos, Unique<TypeAst> output_type, Unique<TypeAst> residual_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(output_type)
    ADD_TYPE_GENERIC(residual_type)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Try")
    ADD_NAMESPACE("try")
    ADD_NAMESPACE("ops")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::FutureType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(inner_type)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Fut")
    ADD_NAMESPACE("future")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::OptionType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(inner_type)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Opt")
    ADD_NAMESPACE("opt")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::MemoryType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(inner_type)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Memory")
    ADD_NAMESPACE("memory")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::SingleType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(inner_type)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Single")
    ADD_NAMESPACE("single")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::ViewType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(inner_type)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("View")
    ADD_NAMESPACE("view")
    ADD_NAMESPACE("std")
    ADD_BORROW()
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::SomeType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(inner_type)
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Some")
    ADD_NAMESPACE("option")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::None(std::size_t pos) -> Unique<TypeAst> {
    MAKE_TYPE("None")
    ADD_NAMESPACE("option")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::GenType(std::size_t pos, Unique<TypeAst> yield_type, Unique<TypeAst> send_type) -> Unique<TypeAst> {
    MAKE_GENERICS()
    ADD_TYPE_GENERIC(yield_type)
    ADD_TYPE_GENERIC(send_type != nullptr ? std::move(send_type) : VoidType(pos))
    FINISH_GENERICS()

    MAKE_TYPE_WITH_GN("Gen")
    ADD_NAMESPACE("generator")
    ADD_NAMESPACE("std")
    FINISH_TYPE()
}

auto spp::asts::generate::common_types::GenOnceType(std::size_t pos, Unique<TypeAst> yield_type) -> Unique<TypeAst> {
    // TODO: Finish macro conversions.
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(yield_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("GenOnce"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("generator")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::IndexMutType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("IndexMut"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::IndexRefType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("IndexRef"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SliceMutType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("SliceMut"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SliceRefType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(elem_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("SliceRef"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("idx")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::FunRefType(std::size_t pos, Unique<TypeAst> param_types, Unique<TypeAst> ret_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("FunRef"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("function")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::FunMutType(std::size_t pos, Unique<TypeAst> param_types, Unique<TypeAst> ret_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("FunMut"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("function")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::FunMovType(std::size_t pos, Unique<TypeAst> param_types, Unique<TypeAst> ret_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(2);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(param_types));
    generics_lst[1] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(ret_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("FunMov"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("function")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::ForwardRefType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("FwdRef"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("fwd")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::ForwardMutType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst> {
    auto generics_lst = UniqueVec<GenericArgumentAst>(1);
    generics_lst[0] = MakeUnique<GenericArgumentTypePositionalAst>(std::move(inner_type));
    auto generics = MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(generics_lst), nullptr);

    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("FwdMut"), std::move(generics));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("fwd")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("ops")), nullptr), std::move(type));
    type = MakeUnique<TypeUnaryExpressionAst>(MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(MakeUnique<IdentifierAst>(pos, Str("std")), nullptr), std::move(type));
    return type;
}

auto spp::asts::generate::common_types::SelfType(std::size_t pos) -> Unique<TypeAst> {
    Unique<TypeAst> type = MakeUnique<TypeIdentifierAst>(pos, Str("Self"), nullptr);
    return type;
}
