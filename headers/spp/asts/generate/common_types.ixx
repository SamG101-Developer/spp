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
    SPP_EXP_FUN auto F8(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto F16(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto F32(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto F64(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto F128(std::size_t pos) -> Shared<TypeAst>;

    SPP_EXP_FUN auto S8(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto S16(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto S32(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto S64(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto S128(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto S256(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto SSize(std::size_t pos) -> Shared<TypeAst>;

    SPP_EXP_FUN auto U8(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto U16(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto U32(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto U64(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto U128(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto U256(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto USize(std::size_t pos) -> Shared<TypeAst>;

    SPP_EXP_FUN auto VoidType(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto BooleanType(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto StringType(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto StringViewType(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto NeverType(std::size_t pos) -> Shared<TypeAst>;
    SPP_EXP_FUN auto CopyType(std::size_t pos) -> Shared<TypeAst>;

    SPP_EXP_FUN auto ArrayType(std::size_t pos, Shared<TypeAst> elem_type, Unique<ExpressionAst> &&size) -> Shared<TypeAst>;
    SPP_EXP_FUN auto VariantType(std::size_t pos, SharedVec<TypeAst> &&inner_types) -> Shared<TypeAst>;
    SPP_EXP_FUN auto TupleType(std::size_t pos, SharedVec<TypeAst> &&inner_types) -> Shared<TypeAst>;
    SPP_EXP_FUN auto TryType(std::size_t pos, Shared<TypeAst> output_type, Shared<TypeAst> residual_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto FutureType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto OptionType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto MemoryType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto SingleType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst>;

    SPP_EXP_FUN auto SomeType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto None(std::size_t pos) -> Shared<TypeAst>;

    SPP_EXP_FUN auto GenType(std::size_t pos, Shared<TypeAst> yield_type, Shared<TypeAst> send_type = nullptr) -> Shared<TypeAst>;
    SPP_EXP_FUN auto GenOnceType(std::size_t pos, Shared<TypeAst> yield_type) -> Shared<TypeAst>;

    SPP_EXP_FUN auto IndexMutType(std::size_t pos, Shared<TypeAst> elem_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto IndexRefType(std::size_t pos, Shared<TypeAst> elem_type) -> Shared<TypeAst>;

    SPP_EXP_FUN auto FunRefType(std::size_t pos, Shared<TypeAst> param_types, Shared<TypeAst> ret_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto FunMutType(std::size_t pos, Shared<TypeAst> param_types, Shared<TypeAst> ret_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto FunMovType(std::size_t pos, Shared<TypeAst> param_types, Shared<TypeAst> ret_type) -> Shared<TypeAst>;

    SPP_EXP_FUN auto ForwardRefType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst>;
    SPP_EXP_FUN auto ForwardMutType(std::size_t pos, Shared<TypeAst> inner_type) -> Shared<TypeAst>;

    SPP_EXP_FUN auto SelfType(std::size_t pos) -> Shared<TypeAst>;
}
