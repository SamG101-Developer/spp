#include <spp/codegen/llvm_func_impls.hpp>
#include <spp/codegen/llvm_mangle.hpp>
#include <spp/utils/files.hpp>

// static const auto LLVM_PATH = std::filesystem::path(__FILE__).parent_path() / "raw_impl";
//
//
// auto spp::codegen::func_impls::modify_code(
//     std::string &code,
//     std::map<std::string, std::string> const &params)
//     -> void {
//     // For each key-value pair in params, replace occurrences of the key in code with the value.
//     for (auto const &[k, v] : params) {
//         auto pos = 0uz;
//         while ((pos = code.find(k)) != std::string::npos) {
//             code.replace(pos, k.size(), v);
//         }
//     }
// }
//
//
// auto spp::codegen::func_impls::std_array_arr_iter_mov(
//     std::string const &llvm_elem_type,
//     std::string const &llvm_size)
//     -> std::string {
//     // Pull from std_array_arr_iter_mov.ll and read into a string.
//     auto code = utils::files::read_file(LLVM_PATH / "std_array_arr_iter_mov.ll");
//     modify_code(code, {{"__T", llvm_elem_type}, {"__n", llvm_size}});
//     return code;
// }
//
//
// auto spp::codegen::func_impls::std_array_arr_iter_mut(
//     std::string const &llvm_elem_type,
//     std::string const &llvm_size)
//     -> std::string {
//     // Pull from std_array_arr_iter_mut.ll and read into a string.
//     auto code = utils::files::read_file(LLVM_PATH / "std_array_arr_iter_mut.ll");
//     modify_code(code, {{"__T", llvm_elem_type}, {"__n", llvm_size}});
//     return code;
// }
//
//
// auto spp::codegen::func_impls::std_array_arr_iter_ref(
//     std::string const &llvm_elem_type,
//     std::string const &llvm_size)
//     -> std::string {
//     // Pull from std_array_arr_iter_ref.ll and read into a string.
//     auto code = utils::files::read_file(LLVM_PATH / "std_array_arr_iter_ref.ll");
//     modify_code(code, {{"__T", llvm_elem_type}, {"__n", llvm_size}});
//     return code;
// }


#define MAKE_BIN_OP_CLOSURE(Func) \
    [&ctx](llvm::Value *a, llvm::Value *b) { return ctx->builder.Func(a, b, "result"); }


#define MAKE_UN_OP_CLOSURE(Func) \
    [&ctx](llvm::Value *a) { return ctx->builder.Func(a, "result"); }


#define MAKE_CONV_CLOSURE(Func) \
    [&ctx, ty](llvm::Value *a) { return ctx->builder.Func(a, ty, "result"); }


auto spp::codegen::func_impls::std_abort_abort(LLvmCtx *ctx) -> void {
    // Define types that will be used for the function signature.
    const auto void_ty = llvm::Type::getVoidTy(ctx->context);
    const auto i8_ptr_ty = llvm::PointerType::get(ctx->context, 0);
    const auto i64_ty = llvm::Type::getInt64Ty(ctx->context);

    // Create the function type and function.
    const auto fn_type = llvm::FunctionType::get(void_ty, {i8_ptr_ty, i64_ty}, false);
    const auto fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, "abort", ctx->module.get());
    fn->addFnAttr(llvm::Attribute::NoReturn);

    // Create the entry basic block.
    const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);
    const auto trap_intrinsic = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::trap);
    ctx->builder.SetInsertPoint(entry_bb);
    ctx->builder.CreateCall(trap_intrinsic, {});
    ctx->builder.CreateUnreachable();
}


auto spp::codegen::func_impls::std_boolean_bit_and(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAnd);
    simple_intrinsic_binop(ctx, llvm::Type::getInt1Ty(ctx->context), "add", closure);
}


auto spp::codegen::func_impls::std_boolean_bit_ior(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateOr);
    simple_intrinsic_binop(ctx, llvm::Type::getInt1Ty(ctx->context), "ior", closure);
}


auto spp::codegen::func_impls::std_boolean_bit_xor(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateXor);
    simple_intrinsic_binop(ctx, llvm::Type::getInt1Ty(ctx->context), "xor", closure);
}


auto spp::codegen::func_impls::std_boolean_and(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLogicalAnd);
    simple_intrinsic_binop(ctx, llvm::Type::getInt1Ty(ctx->context), "and", closure);
}


auto spp::codegen::func_impls::std_boolean_ior(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLogicalOr);
    simple_intrinsic_binop(ctx, llvm::Type::getInt1Ty(ctx->context), "ior", closure);
}


auto spp::codegen::func_impls::std_boolean_not(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_UN_OP_CLOSURE(CreateNot);
    simple_intrinsic_unop(ctx, llvm::Type::getInt1Ty(ctx->context), "not", closure);
}


auto spp::codegen::func_impls::std_boolean_eq(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpEQ);
    simple_intrinsic_binop(ctx, llvm::Type::getInt1Ty(ctx->context), "eq", closure);
}


auto spp::codegen::func_impls::std_boolean_ne(LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpNE);
    simple_intrinsic_binop(ctx, llvm::Type::getInt1Ty(ctx->context), "ne", closure);
}


auto spp::codegen::func_impls::std_intrinsics_add(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAdd);
    simple_intrinsic_binop(ctx, ty, "add", closure);
}


auto spp::codegen::func_impls::std_intrinsics_add_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAdd);
    simple_intrinsic_binop_assign(ctx, ty, "add_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsics_sub(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSub);
    simple_intrinsic_binop(ctx, ty, "sub", closure);
}


auto spp::codegen::func_impls::std_intrinsics_sub_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSub);
    simple_intrinsic_binop_assign(ctx, ty, "sub_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_mul(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateMul);
    simple_intrinsic_binop(ctx, ty, "mul", closure);
}


auto spp::codegen::func_impls::std_intrinsic_mul_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateMul);
    simple_intrinsic_binop_assign(ctx, ty, "mul_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_sdiv(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSDiv);
    simple_intrinsic_binop(ctx, ty, "sdiv", closure);
}


auto spp::codegen::func_impls::std_intrinsic_sdiv_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSDiv);
    simple_intrinsic_binop_assign(ctx, ty, "sdiv_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_udiv(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateUDiv);
    simple_intrinsic_binop(ctx, ty, "udiv", closure);
}


auto spp::codegen::func_impls::std_intrinsic_udiv_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateUDiv);
    simple_intrinsic_binop_assign(ctx, ty, "udiv_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_srem(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSRem);
    simple_intrinsic_binop(ctx, ty, "srem", closure);
}


auto spp::codegen::func_impls::std_intrinsic_srem_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSRem);
    simple_intrinsic_binop_assign(ctx, ty, "srem_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_urem(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateURem);
    simple_intrinsic_binop(ctx, ty, "urem", closure);
}


auto spp::codegen::func_impls::std_intrinsic_urem_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateURem);
    simple_intrinsic_binop_assign(ctx, ty, "urem_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_sneg(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_UN_OP_CLOSURE(CreateNeg);
    simple_intrinsic_unop(ctx, ty, "sneg", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shl(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateShl);
    simple_intrinsic_binop(ctx, ty, "bit_shl", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shl_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateShl);
    simple_intrinsic_binop_assign(ctx, ty, "bit_shl_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shr(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLShr);
    simple_intrinsic_binop(ctx, ty, "bit_shr", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shr_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLShr);
    simple_intrinsic_binop_assign(ctx, ty, "bit_shr_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_ior(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateOr);
    simple_intrinsic_binop(ctx, ty, "ior", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_ior_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateOr);
    simple_intrinsic_binop_assign(ctx, ty, "ior_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_and(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAnd);
    simple_intrinsic_binop(ctx, ty, "and", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_and_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAnd);
    simple_intrinsic_binop_assign(ctx, ty, "and_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_xor(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateXor);
    simple_intrinsic_binop(ctx, ty, "xor", closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_xor_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateXor);
    simple_intrinsic_binop_assign(ctx, ty, "xor_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_abs(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::abs;
    simple_unary_intrinsic_call(ctx, ty, "abs", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_eq(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpEQ);
    simple_intrinsic_binop(ctx, ty, "eq", closure);
}


auto spp::codegen::func_impls::std_intrinsic_ne(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpNE);
    simple_intrinsic_binop(ctx, ty, "ne", closure);
}


auto spp::codegen::func_impls::std_intrinsic_slt(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSLT);
    simple_intrinsic_binop(ctx, ty, "slt", closure);
}


auto spp::codegen::func_impls::std_intrinsic_ult(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpULT);
    simple_intrinsic_binop(ctx, ty, "ult", closure);
}


auto spp::codegen::func_impls::std_intrinsic_sle(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSLE);
    simple_intrinsic_binop(ctx, ty, "sle", closure);
}


auto spp::codegen::func_impls::std_intrinsic_ule(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpULE);
    simple_intrinsic_binop(ctx, ty, "ule", closure);
}


auto spp::codegen::func_impls::std_intrinsic_sgt(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSGT);
    simple_intrinsic_binop(ctx, ty, "sgt", closure);
}


auto spp::codegen::func_impls::std_intrinsic_ugt(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpUGT);
    simple_intrinsic_binop(ctx, ty, "ugt", closure);
}


auto spp::codegen::func_impls::std_intrinsic_sge(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSGE);
    simple_intrinsic_binop(ctx, ty, "sge", closure);
}


auto spp::codegen::func_impls::std_intrinsic_uge(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpUGE);
    simple_intrinsic_binop(ctx, ty, "uge", closure);
}


auto spp::codegen::func_impls::std_intrinsic_smin(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::smin;
    simple_unary_intrinsic_call(ctx, ty, "smin", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_smax(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::smax;
    simple_unary_intrinsic_call(ctx, ty, "smax", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_umin(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::umin;
    simple_unary_intrinsic_call(ctx, ty, "umin", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_umax(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::umax;
    simple_unary_intrinsic_call(ctx, ty, "umax", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fadd(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFAdd);
    simple_intrinsic_binop(ctx, ty, "fadd", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fadd_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFAdd);
    simple_intrinsic_binop_assign(ctx, ty, "fadd_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fsub(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFSub);
    simple_intrinsic_binop(ctx, ty, "fsub", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fsub_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFSub);
    simple_intrinsic_binop_assign(ctx, ty, "fsub_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fmul(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFMul);
    simple_intrinsic_binop(ctx, ty, "fmul", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fmul_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFMul);
    simple_intrinsic_binop_assign(ctx, ty, "fmul_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fdiv(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFDiv);
    simple_intrinsic_binop(ctx, ty, "fdiv", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fdiv_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFDiv);
    simple_intrinsic_binop_assign(ctx, ty, "fdiv_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_frem(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFRem);
    simple_intrinsic_binop(ctx, ty, "frem", closure);
}


auto spp::codegen::func_impls::std_intrinsic_frem_assign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFRem);
    simple_intrinsic_binop_assign(ctx, ty, "frem_assign", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fneg(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_UN_OP_CLOSURE(CreateFNeg);
    simple_intrinsic_unop(ctx, ty, "fneg", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fsqrt(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sqrt;
    simple_unary_intrinsic_call(ctx, ty, "fsqrt", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fpowi(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::powi;
    simple_binary_intrinsic_call(ctx, ty, "fpowi", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fpowf(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::pow;
    simple_binary_intrinsic_call(ctx, ty, "fpowf", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fsin(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sin;
    simple_unary_intrinsic_call(ctx, ty, "fsin", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fcos(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::cos;
    simple_unary_intrinsic_call(ctx, ty, "fcos", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ftan(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::tan;
    simple_unary_intrinsic_call(ctx, ty, "ftan", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fasin(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::asin;
    simple_unary_intrinsic_call(ctx, ty, "fasin", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_facos(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::acos;
    simple_unary_intrinsic_call(ctx, ty, "facos", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fatan(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::atan;
    simple_unary_intrinsic_call(ctx, ty, "fatan", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fatan2(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::atan2;
    simple_binary_intrinsic_call(ctx, ty, "fatan2", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fsinh(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sinh;
    simple_unary_intrinsic_call(ctx, ty, "fsinh", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fcosh(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::cosh;
    simple_unary_intrinsic_call(ctx, ty, "fcosh", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ftanh(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::tanh;
    simple_unary_intrinsic_call(ctx, ty, "ftanh", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fexp(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::exp;
    simple_unary_intrinsic_call(ctx, ty, "fexp", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fexp2(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::exp2;
    simple_unary_intrinsic_call(ctx, ty, "fexp2", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fexp10(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::exp10;
    simple_unary_intrinsic_call(ctx, ty, "fexp10", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fln(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::log;
    simple_unary_intrinsic_call(ctx, ty, "flog", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_flog2(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::log2;
    simple_unary_intrinsic_call(ctx, ty, "flog2", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_flog10(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::log10;
    simple_unary_intrinsic_call(ctx, ty, "flog10", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fabs(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::fabs;
    simple_unary_intrinsic_call(ctx, ty, "fabs", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fmax(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::maxnum;
    simple_binary_intrinsic_call(ctx, ty, "fmax", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fmin(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::minnum;
    simple_binary_intrinsic_call(ctx, ty, "fmin", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fcopysign(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::copysign;
    simple_binary_intrinsic_call(ctx, ty, "fcopysign", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ffloor(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::floor;
    simple_unary_intrinsic_call(ctx, ty, "ffloor", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fceil(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ceil;
    simple_unary_intrinsic_call(ctx, ty, "fceil", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ftrunc(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::trunc;
    simple_unary_intrinsic_call(ctx, ty, "ftrunc", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fround(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::round;
    simple_unary_intrinsic_call(ctx, ty, "fround", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fbitreverse(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::bitreverse;
    simple_unary_intrinsic_call(ctx, ty, "fbitreverse", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ctlz(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ctlz;
    simple_unary_intrinsic_call(ctx, ty, "ctlz", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sadd_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sadd_with_overflow;
    simple_binary_intrinsic_call(ctx, ty, "sadd_overflow", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_uadd_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::uadd_with_overflow;
    simple_binary_intrinsic_call(ctx, ty, "uadd_overflow", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ssub_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ssub_with_overflow;
    simple_binary_intrinsic_call(ctx, ty, "ssub_overflow", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_usub_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::usub_with_overflow;
    simple_binary_intrinsic_call(ctx, ty, "usub_overflow", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_smul_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::smul_with_overflow;
    simple_binary_intrinsic_call(ctx, ty, "smul_overflow", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_umul_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::umul_with_overflow;
    simple_binary_intrinsic_call(ctx, ty, "umul_overflow", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sadd_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sadd_sat;
    simple_binary_intrinsic_call(ctx, ty, "sadd_saturating", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_uadd_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::uadd_sat;
    simple_binary_intrinsic_call(ctx, ty, "uadd_saturating", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ssub_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ssub_sat;
    simple_binary_intrinsic_call(ctx, ty, "ssub_saturating", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_usub_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::usub_sat;
    simple_binary_intrinsic_call(ctx, ty, "usub_saturating", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sshl_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sshl_sat;
    simple_binary_intrinsic_call(ctx, ty, "sshl_saturating", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ushl_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ushl_sat;
    simple_binary_intrinsic_call(ctx, ty, "ushl_saturating", intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sadd_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNSWAdd);
    simple_intrinsic_binop(ctx, ty, "sadd_wrapping", closure);
}


auto spp::codegen::func_impls::std_intrinsic_uadd_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNUWAdd);
    simple_intrinsic_binop(ctx, ty, "uadd_wrapping", closure);
}


auto spp::codegen::func_impls::std_intrinsic_ssub_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNSWSub);
    simple_intrinsic_binop(ctx, ty, "ssub_wrapping", closure);
}


auto spp::codegen::func_impls::std_intrinsic_usub_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNUWSub);
    simple_intrinsic_binop(ctx, ty, "usub_wrapping", closure);
}


auto spp::codegen::func_impls::std_intrinsic_smul_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNSWMul);
    simple_intrinsic_binop(ctx, ty, "smul_wrapping", closure);
}


auto spp::codegen::func_impls::std_intrinsic_umul_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNUWMul);
    simple_intrinsic_binop(ctx, ty, "umul_wrapping", closure);
}


auto spp::codegen::func_impls::std_intrinsic_sitofp(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateSIToFP);
    simple_intrinsic_unop(ctx, ty, "sitofp", closure);
}


auto spp::codegen::func_impls::std_intrinsic_uitofp(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateUIToFP);
    simple_intrinsic_unop(ctx, ty, "uitofp", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fptrunc(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPTrunc);
    simple_intrinsic_unop(ctx, ty, "fptrunc", closure);
}


auto spp::codegen::func_impls::std_intrinsic_strunc(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateTrunc);
    simple_intrinsic_unop(ctx, ty, "strunc", closure);
}


auto spp::codegen::func_impls::std_intrinsic_utrunc(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateTrunc);
    simple_intrinsic_unop(ctx, ty, "utrunc", closure);
}


auto spp::codegen::func_impls::std_intrinsic_szext(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateSExt);
    simple_intrinsic_unop(ctx, ty, "szext", closure);
}


auto spp::codegen::func_impls::std_intrinsic_uzext(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateZExt);
    simple_intrinsic_unop(ctx, ty, "uzext", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fpext(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPExt);
    simple_intrinsic_unop(ctx, ty, "fpext", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fptosi(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPToSI);
    simple_intrinsic_unop(ctx, ty, "fptosi", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fptoui(LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPToUI);
    simple_intrinsic_unop(ctx, ty, "fptoui", closure);
}


auto spp::codegen::func_impls::std_intrinsic_fpclass(LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::is_fpclass;
    simple_binary_intrinsic_call(ctx, ty, "fpclass", intrinsic);
}
