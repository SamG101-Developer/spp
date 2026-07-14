module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types;
import spp.utils.types;
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
    SPP_EXP_FUN auto F8(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto F16(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto F32(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto F64(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto F128(std::size_t pos) -> Unique<TypeAst>;

    SPP_EXP_FUN auto S8(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto S16(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto S32(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto S64(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto S128(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto S256(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto SSize(std::size_t pos) -> Unique<TypeAst>;

    SPP_EXP_FUN auto U8(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto U16(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto U32(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto U64(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto U128(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto U256(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto USize(std::size_t pos) -> Unique<TypeAst>;

    SPP_EXP_FUN auto CharType(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto VoidType(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto BooleanType(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto StringType(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto StringViewType(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto NeverType(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto CopyType(std::size_t pos) -> Unique<TypeAst>;

    SPP_EXP_FUN auto ArrayType(std::size_t pos, Unique<TypeAst> elem_type, Unique<ExpressionAst> &&size) -> Unique<TypeAst>;
    SPP_EXP_FUN auto ViewU8Type(std::size_t pos) -> Unique<TypeAst>;
    SPP_EXP_FUN auto VariantType(std::size_t pos, UniqueVec<TypeAst> &&inner_types) -> Unique<TypeAst>;
    SPP_EXP_FUN auto TupleType(std::size_t pos, UniqueVec<TypeAst> &&inner_types) -> Unique<TypeAst>;
    SPP_EXP_FUN auto TryType(std::size_t pos, Unique<TypeAst> output_type, Unique<TypeAst> residual_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto FutureType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto OptionType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto MemoryType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto SingleType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto ViewType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;

    SPP_EXP_FUN auto SomeType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto None(std::size_t pos) -> Unique<TypeAst>;

    SPP_EXP_FUN auto GenType(std::size_t pos, Unique<TypeAst> yield_type, Unique<TypeAst> send_type = nullptr) -> Unique<TypeAst>;
    SPP_EXP_FUN auto GenOnceType(std::size_t pos, Unique<TypeAst> yield_type) -> Unique<TypeAst>;

    SPP_EXP_FUN auto IndexMutType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto IndexRefType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst>;

    SPP_EXP_FUN auto SliceMutType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto SliceRefType(std::size_t pos, Unique<TypeAst> elem_type) -> Unique<TypeAst>;

    SPP_EXP_FUN auto FunRefType(std::size_t pos, Unique<TypeAst> param_types, Unique<TypeAst> ret_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto FunMutType(std::size_t pos, Unique<TypeAst> param_types, Unique<TypeAst> ret_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto FunMovType(std::size_t pos, Unique<TypeAst> param_types, Unique<TypeAst> ret_type) -> Unique<TypeAst>;

    SPP_EXP_FUN auto ForwardRefType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;
    SPP_EXP_FUN auto ForwardMutType(std::size_t pos, Unique<TypeAst> inner_type) -> Unique<TypeAst>;

    SPP_EXP_FUN auto SelfType(std::size_t pos) -> Unique<TypeAst>;
}
