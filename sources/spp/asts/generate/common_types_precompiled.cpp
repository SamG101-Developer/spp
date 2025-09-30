#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>


auto spp::asts::generate::common_types_precompiled::initialize_types() -> void {
    GEN = common_types::gen_type(0, common_types::void_type(0), common_types::void_type(0))->without_generics();
    GEN_OPT = common_types::gen_opt_type(0, common_types::void_type(0), common_types::void_type(0))->without_generics();
    GEN_RES = common_types::gen_res_type(0, common_types::void_type(0), common_types::void_type(0), common_types::void_type(0))->without_generics();
    GEN_ONCE = common_types::gen_once_type(0, common_types::void_type(0))->without_generics();

    FUN_MOV = common_types::fun_mov_type(0, common_types::tuple_type(0, {}), common_types::void_type(0))->without_generics();
    FUN_MUT = common_types::fun_mut_type(0, common_types::tuple_type(0, {}), common_types::void_type(0))->without_generics();
    FUN_REF = common_types::fun_ref_type(0, common_types::tuple_type(0, {}), common_types::void_type(0))->without_generics();

    ARR = common_types::array_type(0, common_types::void_type(0), ast_clone(common_types::usize(0)))->without_generics();
    TUP = common_types::tuple_type(0, {})->without_generics();
    VAR = common_types::variant_type(0, {})->without_generics();
    TRY = common_types::try_type(0, common_types::void_type(0), common_types::void_type(0))->without_generics();
    FUT = common_types::future_type(0, common_types::void_type(0))->without_generics();

    BOOL = common_types::boolean_type(0)->without_generics();
    VOID = common_types::void_type(0)->without_generics();

    NEVER = common_types::never_type(0)->without_generics();
    COPY = common_types::copy_type(0)->without_generics();
}
