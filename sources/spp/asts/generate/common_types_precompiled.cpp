module spp.asts.generate.common_types_precompiled;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;


auto spp::asts::generate::common_types_precompiled::ClearTypes() -> void {
    GEN = nullptr; GEN_ONCE = nullptr;
    INDEX_REF = nullptr; INDEX_MUT = nullptr;
    FUN_MOV = nullptr; FUN_MUT = nullptr; FUN_REF = nullptr;
    ARR = nullptr; TUP = nullptr; VAR = nullptr; TRY = nullptr; FUT = nullptr;
    BOOL = nullptr; VOID = nullptr;
    NEVER = nullptr; COPY = nullptr;
    SINGLE = nullptr; SHARED = nullptr; SHADOW = nullptr;
    FWD_MUT = nullptr; FWD_REF = nullptr;
    S8 = nullptr; S16 = nullptr; S32 = nullptr; S64 = nullptr; S128 = nullptr; S256 = nullptr; SSIZE = nullptr;
    U8 = nullptr; U16 = nullptr; U32 = nullptr; U64 = nullptr; U128 = nullptr; U256 = nullptr; USIZE = nullptr;
    F8 = nullptr; F16 = nullptr; F32 = nullptr; F64 = nullptr; F128 = nullptr;
}

auto spp::asts::generate::common_types_precompiled::InitTypes() -> void {
    GEN = common_types::GenType(0, common_types::VoidType(0), common_types::VoidType(0))->WithoutGenerics();
    GEN_ONCE = common_types::GenOnceType(0, common_types::VoidType(0))->WithoutGenerics();

    INDEX_MUT = common_types::IndexMutType(0, common_types::VoidType(0))->WithoutGenerics();
    INDEX_REF = common_types::IndexRefType(0, common_types::VoidType(0))->WithoutGenerics();

    FUN_MOV = common_types::FunMovType(0, common_types::TupleType(0, {}), common_types::VoidType(0))->WithoutGenerics();
    FUN_MUT = common_types::FunMutType(0, common_types::TupleType(0, {}), common_types::VoidType(0))->WithoutGenerics();
    FUN_REF = common_types::FunRefType(0, common_types::TupleType(0, {}), common_types::VoidType(0))->WithoutGenerics();

    ARR = common_types::ArrayType(0, common_types::VoidType(0), AstClone(common_types::USize(0)))->WithoutGenerics();
    TUP = common_types::TupleType(0, {})->WithoutGenerics();
    VAR = common_types::VariantType(0, {})->WithoutGenerics();
    TRY = common_types::TryType(0, common_types::VoidType(0), common_types::VoidType(0))->WithoutGenerics();
    FUT = common_types::FutureType(0, common_types::VoidType(0))->WithoutGenerics();

    BOOL = common_types::BooleanType(0)->WithoutGenerics();
    VOID = common_types::VoidType(0)->WithoutGenerics();

    NEVER = common_types::NeverType(0)->WithoutGenerics();
    COPY = common_types::CopyType(0)->WithoutGenerics();

    FWD_MUT = common_types::ForwardMutType(0, common_types::VoidType(0))->WithoutGenerics();
    FWD_REF = common_types::ForwardRefType(0, common_types::VoidType(0))->WithoutGenerics();
}
