module spp.analyse.utils.builtins;
import spp.analyse.utils.cmp_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.float_literal_ast;
import spp.asts.integer_literal_ast;
import spp.codegen.llvm_func_impls;


// .llvm_fn=spp::utils::functions::make_callable(codegen::func_impls::func_name)
#define SPP_DEFINE_BUILTIN_FUNC(scoped_name, func_name) \
    map.emplace(scoped_name, LoweredFuncImpl{           \
        .cmp_fn=nullptr})

// .llvm_fn=spp::utils::functions::make_callable(codegen::func_impls::func_name),
#define SPP_DEFINE_BUILTIN_FUNC_CMP(scoped_name, func_name) \
    map.emplace(scoped_name, LoweredFuncImpl{               \
        .cmp_fn=cmp_utils::make_cmp_fn(cmp_utils::func_name)})


auto spp::analyse::utils::builtins::make_builtin_functions_map()
    -> std::unordered_map<std::string, LoweredFuncImpl> {
    auto map = std::unordered_map<std::string, LoweredFuncImpl>{};
    SPP_DEFINE_BUILTIN_FUNC("std::abort::abort", std_abort_abort);

    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::iter_mov", std_array_iter_mov);
    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::iter_mut", std_array_iter_mut);
    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::iter_ref", std_array_iter_ref);
    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::reverse_iter_mov", std_array_reverse_iter_mov);
    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::reverse_iter_mut", std_array_reverse_iter_mut);
    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::reverse_iter_ref", std_array_reverse_iter_ref);
    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::index_mut", std_array_index_mut);
    SPP_DEFINE_BUILTIN_FUNC("std::array::Arr::index_ref", std_array_index_ref);

    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::bit_and", std_boolean_bit_and);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::bit_ior", std_boolean_bit_ior);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::bit_xor", std_boolean_bit_xor);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::bit_not", std_boolean_bit_not);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::and", std_boolean_and);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::ior", std_boolean_ior);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::eq", std_boolean_eq);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::boolean::Bool::ne", std_boolean_ne);

    SPP_DEFINE_BUILTIN_FUNC("std::cast::upcast_mov", std_cast_upcast_mov);
    SPP_DEFINE_BUILTIN_FUNC("std::cast::upcast_mut", std_cast_upcast_mut);
    SPP_DEFINE_BUILTIN_FUNC("std::cast::upcast_ref", std_cast_upcast_ref);
    SPP_DEFINE_BUILTIN_FUNC("std::cast::downcast_mov", std_cast_downcast_mov);
    SPP_DEFINE_BUILTIN_FUNC("std::cast::downcast_mut", std_cast_downcast_mut);
    SPP_DEFINE_BUILTIN_FUNC("std::cast::downcast_ref", std_cast_downcast_ref);

    SPP_DEFINE_BUILTIN_FUNC("std::generator::Gen::send", std_generator_send);
    SPP_DEFINE_BUILTIN_FUNC("std::generator::GenOnce::send", std_generator_once_send);

    SPP_DEFINE_BUILTIN_FUNC("std::memory::clear", std_memory_clear);
    SPP_DEFINE_BUILTIN_FUNC("std::memory::place_element", std_memory_place_element);
    SPP_DEFINE_BUILTIN_FUNC("std::memory::take_element", std_memory_take_element);
    SPP_DEFINE_BUILTIN_FUNC("std::memops::sizeof", std_memops_sizeof);
    // SPP_DEFINE_BUILTIN_FUNC("std::memops::sizeof_value", std_memops_sizeof_value);

    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::add", std_intrinsics_add);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::add_assign", std_intrinsics_add_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sub", std_intrinsics_sub);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sub_assign", std_intrinsics_sub_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::mul", std_intrinsics_mul);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::mul_assign", std_intrinsics_mul_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sdiv", std_intrinsics_sdiv);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sdiv_assign", std_intrinsics_sdiv_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::udiv", std_intrinsics_udiv);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::udiv_assign", std_intrinsics_udiv_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::srem", std_intrinsics_srem);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::srem_assign", std_intrinsics_srem_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::urem", std_intrinsics_urem);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::urem_assign", std_intrinsics_urem_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sneg", std_intrinsics_sneg);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_shl", std_intrinsics_bit_shl);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_shl_assign", std_intrinsics_bit_shl_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_shr", std_intrinsics_bit_shr);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_shr_assign", std_intrinsics_bit_shr_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_ior", std_intrinsics_bit_ior);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_ior_assign", std_intrinsics_bit_ior_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_and", std_intrinsics_bit_and);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_and_assign", std_intrinsics_bit_and_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_xor", std_intrinsics_bit_xor);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_xor_assign", std_intrinsics_bit_xor_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_not", std_intrinsics_bit_not);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::bit_not_assign", std_intrinsics_bit_not_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::abs", std_intrinsics_abs);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::eq", std_intrinsics_eq);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::oeq", std_intrinsics_oeq);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::ne", std_intrinsics_ne);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::one", std_intrinsics_one);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::slt", std_intrinsics_slt);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::ult", std_intrinsics_ult);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::olt", std_intrinsics_olt);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sle", std_intrinsics_sle);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::ule", std_intrinsics_ule);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::ole", std_intrinsics_ole);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sgt", std_intrinsics_sgt);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::ugt", std_intrinsics_ugt);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::ogt", std_intrinsics_ogt);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::sge", std_intrinsics_sge);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::uge", std_intrinsics_uge);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::oge", std_intrinsics_oge);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::min_val", std_intrinsics_min_val);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::max_val", std_intrinsics_max_val);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::smin", std_intrinsics_smin);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::smax", std_intrinsics_smax);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::umin", std_intrinsics_umin);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::umax", std_intrinsics_umax);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_add", std_intrinsics_fadd);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_add_assign", std_intrinsics_fadd_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_sub", std_intrinsics_fsub);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_sub_assign", std_intrinsics_fsub_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_mul", std_intrinsics_fmul);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_mul_assign", std_intrinsics_fmul_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_div", std_intrinsics_fdiv);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_div_assign", std_intrinsics_fdiv_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_rem", std_intrinsics_frem);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_rem_assign", std_intrinsics_frem_assign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_neg", std_intrinsics_fneg);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_sqrt", std_intrinsics_fsqrt);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_powi", std_intrinsics_fpowi);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_powf", std_intrinsics_fpowf);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_sin", std_intrinsics_fsin);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_cos", std_intrinsics_fcos);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_tan", std_intrinsics_ftan);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_asin", std_intrinsics_fasin);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_acos", std_intrinsics_facos);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_atan", std_intrinsics_fatan);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_atan2", std_intrinsics_fatan2);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_sinh", std_intrinsics_fsinh);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_cosh", std_intrinsics_fcosh);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_tanh", std_intrinsics_ftanh);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_exp", std_intrinsics_fexp);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_exp2", std_intrinsics_fexp2);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_exp10", std_intrinsics_fexp10);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_ln", std_intrinsics_fln);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_log2", std_intrinsics_flog2);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_log10", std_intrinsics_flog10);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_abs", std_intrinsics_fabs);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_max_val", std_intrinsics_fmax_val);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_min_val", std_intrinsics_fmin_val);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_max", std_intrinsics_fmax);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_min", std_intrinsics_fmin);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::float_copysign", std_intrinsics_fcopysign);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_floor", std_intrinsics_ffloor);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_ceil", std_intrinsics_fceil);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_trunc", std_intrinsics_ftrunc);
    SPP_DEFINE_BUILTIN_FUNC_CMP("std::intrinsics::float_round", std_intrinsics_fround);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::bitreverse", std_intrinsics_bitreverse);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::ctlz", std_intrinsics_ctlz);

    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::sadd_overflow", std_intrinsics_sadd_overflow);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::uadd_overflow", std_intrinsics_uadd_overflow);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::ssub_overflow", std_intrinsics_ssub_overflow);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::usub_overflow", std_intrinsics_usub_overflow);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::smul_overflow", std_intrinsics_smul_overflow);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::umul_overflow", std_intrinsics_umul_overflow);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::sadd_saturating", std_intrinsics_sadd_saturating);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::uadd_saturating", std_intrinsics_uadd_saturating);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::ssub_saturating", std_intrinsics_ssub_saturating);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::usub_saturating", std_intrinsics_usub_saturating);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::sshl_saturating", std_intrinsics_sshl_saturating);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::ushl_saturating", std_intrinsics_ushl_saturating);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::sadd_wrapping", std_intrinsics_sadd_wrapping);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::uadd_wrapping", std_intrinsics_uadd_wrapping);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::ssub_wrapping", std_intrinsics_ssub_wrapping);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::usub_wrapping", std_intrinsics_usub_wrapping);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::smul_wrapping", std_intrinsics_smul_wrapping);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::umul_wrapping", std_intrinsics_umul_wrapping);

    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::sitofp", std_intrinsics_sitofp);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::uitofp", std_intrinsics_uitofp);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::fptrunc", std_intrinsics_fptrunc);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::strunc", std_intrinsics_strunc);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::utrunc", std_intrinsics_utrunc);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::szext", std_intrinsics_szext);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::uzext", std_intrinsics_uzext);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::fpext", std_intrinsics_fpext);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::bit_cast", std_intrinsics_bit_cast);

    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::fptosi", std_intrinsics_fptosi);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::fptoui", std_intrinsics_fptoui);
    SPP_DEFINE_BUILTIN_FUNC("std::intrinsics::fpclass", std_intrinsics_fpclass);
    return map;
}
