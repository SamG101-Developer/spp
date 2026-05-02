module spp.asts.generate.common_types_precompiled;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;


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
