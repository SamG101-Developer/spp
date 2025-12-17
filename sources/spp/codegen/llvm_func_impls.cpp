module;
#include <spp/macros.hpp>

module spp.codegen.llvm_func_impls;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.generate.common_types;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_size;
import spp.codegen.llvm_type;

import llvm;


template <typename F>
auto spp::codegen::func_impls::simple_intrinsic_binop(
    analyse::scopes::ScopeManager const *sm,
    asts::FunctionPrototypeAst const *proto,
    LLvmCtx *ctx,
    llvm::Type *ty,
    F &&method)
    -> void {
    // Create the binary function.
    const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
    const auto fn_ty = llvm::FunctionType::get(ty, {ty, ty}, false);
    const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);

    // Build the function body.
    ctx->builder.SetInsertPoint(entry_bb);
    const auto lhs = fn->arg_begin();
    const auto rhs = fn->arg_begin() + 1;
    const auto result = method(lhs, rhs);
    ctx->builder.CreateRet(result);
}


template <typename F>
auto spp::codegen::func_impls::simple_intrinsic_binop_assign(
    analyse::scopes::ScopeManager const *sm,
    asts::FunctionPrototypeAst const *proto,
    LLvmCtx *ctx,
    llvm::Type *ty,
    F &&method)
    -> void {
    // Create the add_assign function.
    const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
    const auto ptr_ty = llvm::PointerType::get(*ctx->context, 0);
    const auto fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->context), {ptr_ty, ty}, false);
    const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);

    // Build the function body.
    ctx->builder.SetInsertPoint(entry_bb);
    const auto lhs = fn->arg_begin();
    const auto rhs = fn->arg_begin() + 1;
    SPP_ASSERT(ty != nullptr and lhs != nullptr);
    const auto loaded_val = ctx->builder.CreateLoad(ty, lhs, "loaded_val");
    const auto result = method(loaded_val, rhs);

    SPP_ASSERT(result != nullptr and lhs != nullptr);
    ctx->builder.CreateStore(result, lhs);
    ctx->builder.CreateRetVoid();
}


template <typename F>
auto spp::codegen::func_impls::simple_intrinsic_unop(
    SPP_LLVM_FUNC_INFO,
    LLvmCtx *ctx,
    llvm::Type *ty,
    F &&method)
    -> void {
    // Create the unary function.
    const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
    const auto fn_ty = llvm::FunctionType::get(ty, {ty}, false);
    const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);

    // Build the function body.
    ctx->builder.SetInsertPoint(entry_bb);
    const auto operand = fn->arg_begin();
    const auto result = method(operand);
    ctx->builder.CreateRet(result);
}


auto spp::codegen::func_impls::simple_binary_intrinsic_call(
    SPP_LLVM_FUNC_INFO,
    LLvmCtx *ctx,
    llvm::Type *ty,
    const llvm::Intrinsic::IndependentIntrinsics intrinsic)
    -> void {
    // Create the function.
    const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
    const auto fn_ty = llvm::FunctionType::get(ty, {ty, ty}, false);
    const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);

    // Build the function body.
    ctx->builder.SetInsertPoint(entry_bb);
    const auto lhs = fn->arg_begin();
    const auto rhs = fn->arg_begin() + 1;
    const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), intrinsic, {ty});
    const auto result = ctx->builder.CreateCall(intrinsic_fn, {lhs, rhs}, "result");
    ctx->builder.CreateRet(result);
}


auto spp::codegen::func_impls::simple_unary_intrinsic_call(
    SPP_LLVM_FUNC_INFO,
    LLvmCtx *ctx,
    llvm::Type *ty,
    const llvm::Intrinsic::IndependentIntrinsics intrinsic)
    -> void {
    // Create the function.
    const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
    const auto fn_ty = llvm::FunctionType::get(ty, {ty}, false);
    const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);

    // Build the function body.
    ctx->builder.SetInsertPoint(entry_bb);
    const auto operand = fn->arg_begin();
    const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), intrinsic, {ty});
    const auto result = ctx->builder.CreateCall(intrinsic_fn, {operand}, "result");
    ctx->builder.CreateRet(result);
}


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
//     modify_code(code, {{llvm_elem_type}, {llvm_size}});
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
//     modify_code(code, {{llvm_elem_type}, {llvm_size}});
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
//     modify_code(code, {{llvm_elem_type}, {llvm_size}});
//     return code;
// }


#define MAKE_BIN_OP_CLOSURE(Func) \
    [&ctx](llvm::Value *a, llvm::Value *b) { return ctx->builder.Func(a, b, "result"); }


#define MAKE_UN_OP_CLOSURE(Func) \
    [&ctx](llvm::Value *a) { return ctx->builder.Func(a, "result"); }


#define MAKE_CONV_CLOSURE(Func) \
    [&ctx, ty](llvm::Value *a) { return ctx->builder.Func(a, ty, "result"); }


auto spp::codegen::func_impls::std_abort_abort(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    // Define types that will be used for the function signature.
    const auto void_ty = llvm::Type::getVoidTy(*ctx->context);
    const auto i8_ptr_ty = llvm::PointerType::get(*ctx->context, 0);
    const auto i64_ty = llvm::Type::getInt64Ty(*ctx->context);

    // Create the function type and function.
    const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
    const auto fn_type = llvm::FunctionType::get(void_ty, {i8_ptr_ty, i64_ty}, false);
    const auto fn = llvm::Function::Create(
        fn_type, llvm::Function::ExternalLinkage, name, ctx->module.get());
    fn->addFnAttr(llvm::Attribute::NoReturn);

    // Create the entry basic block.
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);
    const auto trap_intrinsic = llvm::Intrinsic::getOrInsertDeclaration(
        ctx->module.get(), llvm::Intrinsic::trap);
    ctx->builder.SetInsertPoint(entry_bb);
    ctx->builder.CreateCall(trap_intrinsic, {});
    ctx->builder.CreateUnreachable();
}


auto spp::codegen::func_impls::std_boolean_bit_and(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAnd);
    simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_boolean_bit_ior(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateOr);
    simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_boolean_bit_xor(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateXor);
    simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_boolean_and(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLogicalAnd);
    simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_boolean_ior(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLogicalOr);
    simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_boolean_not(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_UN_OP_CLOSURE(CreateNot);
    simple_intrinsic_unop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_boolean_eq(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpEQ);
    simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_boolean_ne(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpNE);
    simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->context), closure);
}


auto spp::codegen::func_impls::std_memory_clear(
    analyse::scopes::ScopeManager const *sm,
    asts::FunctionPrototypeAst const *proto,
    LLvmCtx *ctx,
    std::shared_ptr<asts::TypeAst> const &spp_ty)
    -> void {
    // Define the types that will be used in the function.
    const auto void_ty = llvm::Type::getVoidTy(*ctx->context);
    const auto mem_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::memory_type(0, spp_ty)), ctx);
    const auto usize_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::usize(0)), ctx);

    const auto fn_ty = llvm::FunctionType::get(void_ty, {llvm::PointerType::get(*ctx->context, 0)}, false);
    const auto fn = llvm::Function::Create(
        fn_ty, llvm::Function::ExternalLinkage, mangle::mangle_fun_name(*sm->current_scope, *proto), ctx->module.get());

    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);
    ctx->builder.SetInsertPoint(entry_bb);

    // Extract the argument for "self".
    auto arg_it = fn->args().begin();
    const auto self = arg_it++;

    // Load the pointer from the memory structure.
    const auto int32_ty = llvm::Type::getInt32Ty(*ctx->context);
    const auto const_idx_0 = llvm::ConstantInt::get(int32_ty, 0);
    const auto ptr_addr = ctx->builder.CreateGEP(mem_ty, self, {const_idx_0, const_idx_0}, "ptr_addr");
    const auto ptr_type = llvm::PointerType::get(*ctx->context, 0);
    SPP_ASSERT(ptr_type != nullptr);
    SPP_ASSERT_LLVM_TYPE_OPAQUE(ptr_type);

    const auto ptr = ctx->builder.CreateLoad(ptr_type, ptr_addr, "ptr");

    // Zero out the memory by calling llvm.memset.
    const auto memset_intrinsic = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::memset);
    const auto zero_val = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), 0);
    const auto is_volatile = llvm::ConstantInt::getFalse(*ctx->context);
    ctx->builder.CreateCall(memset_intrinsic, {ptr, zero_val, llvm::ConstantInt::get(usize_ty, 0), is_volatile});
    ctx->builder.CreateRetVoid();
}


auto spp::codegen::func_impls::std_memory_place_element(
    analyse::scopes::ScopeManager const *sm,
    asts::FunctionPrototypeAst const *proto,
    LLvmCtx *ctx,
    std::shared_ptr<asts::TypeAst> const &spp_ty)
    -> void {
    // Define the types that will be used in the function.
    const auto ty = llvm_type(*sm->current_scope->get_type_symbol(spp_ty), ctx);
    const auto void_ty = llvm::Type::getVoidTy(*ctx->context);
    const auto mem_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::memory_type(0, spp_ty)), ctx);
    const auto ptr_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::single_type(0, spp_ty)), ctx);
    const auto size_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::usize(0)), ctx);

    const auto fn_ty = llvm::FunctionType::get(void_ty, {llvm::PointerType::get(*ctx->context, 0), size_ty, ty}, false);
    const auto fn = llvm::Function::Create(
        fn_ty, llvm::Function::ExternalLinkage, mangle::mangle_fun_name(*sm->current_scope, *proto), ctx->module.get());

    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);
    ctx->builder.SetInsertPoint(entry_bb);

    // Extract the arguments for "self", "index", and "value".
    auto arg_it = fn->args().begin();
    const auto self = arg_it++;
    const auto index = arg_it++;
    const auto value = arg_it++;

    // Load the pointer from the memory structure.
    const auto int32_ty = llvm::Type::getInt32Ty(*ctx->context);
    const auto const_idx_0 = llvm::ConstantInt::get(int32_ty, 0);

    SPP_ASSERT(mem_ty != nullptr and ptr_ty != nullptr and ty != nullptr);
    const auto ptr_addr = ctx->builder.CreateGEP(mem_ty, self, {const_idx_0, const_idx_0}, "ptr_addr");
    const auto ptr = ctx->builder.CreateLoad(ptr_ty, ptr_addr, "ptr");
    const auto elem_ptr = ctx->builder.CreateGEP(ty, ptr, {index}, "elem_ptr");

    // Store the value at the computed element pointer.
    SPP_ASSERT(value != nullptr and elem_ptr != nullptr);
    ctx->builder.CreateStore(value, elem_ptr);
    ctx->builder.CreateRetVoid();
}


auto spp::codegen::func_impls::std_memory_take_element(
    analyse::scopes::ScopeManager const *sm,
    asts::FunctionPrototypeAst const *proto,
    LLvmCtx *ctx,
    std::shared_ptr<asts::TypeAst> const &spp_ty)
    -> void {
    // Define the types that will be used in the function.
    const auto ty = llvm_type(*sm->current_scope->get_type_symbol(spp_ty), ctx);
    const auto opt_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::option_type(0, spp_ty)), ctx);
    const auto mem_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::memory_type(0, spp_ty)), ctx);
    const auto ptr_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::single_type(0, spp_ty)), ctx);
    const auto size_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::usize(0)), ctx);

    const auto fn_ty = llvm::FunctionType::get(opt_ty, {llvm::PointerType::get(*ctx->context, 0), size_ty}, false);
    const auto fn = llvm::Function::Create(
        fn_ty, llvm::Function::ExternalLinkage, mangle::mangle_fun_name(*sm->current_scope, *proto), ctx->module.get());

    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);
    ctx->builder.SetInsertPoint(entry_bb);

    // Extract the arguments for "self" and "index".
    auto arg_it = fn->args().begin();
    const auto self = arg_it++;
    const auto index = arg_it++;

    // Load the capacity from the memory structure.
    const auto int32_ty = llvm::Type::getInt32Ty(*ctx->context);
    const auto const_idx_0 = llvm::ConstantInt::get(int32_ty, 0);
    const auto const_idx_1 = llvm::ConstantInt::get(int32_ty, 1);

    SPP_ASSERT(mem_ty != nullptr and size_ty != nullptr);
    const auto cap_addr = ctx->builder.CreateGEP(mem_ty, self, {const_idx_0, const_idx_1}, "cap_addr");
    const auto cap_val = ctx->builder.CreateLoad(size_ty, cap_addr, "cap_val");

    // Check if the index is in bounds.
    const auto cmp = ctx->builder.CreateICmpULT(index, cap_val, "bounds_check");
    const auto then_bb = llvm::BasicBlock::Create(*ctx->context, "in_bounds", fn);
    const auto else_bb = llvm::BasicBlock::Create(*ctx->context, "out_of_bounds", fn);
    const auto exit_bb = llvm::BasicBlock::Create(*ctx->context, "exit", fn);
    ctx->builder.CreateCondBr(cmp, then_bb, else_bb);

    // If in bounds, load the value at the index and return it wrapped in Some.
    ctx->builder.SetInsertPoint(then_bb);
    SPP_ASSERT(mem_ty != nullptr and ptr_ty != nullptr and ty != nullptr);
    const auto ptr_addr = ctx->builder.CreateGEP(mem_ty, self, {const_idx_0, const_idx_1}, "ptr_addr");
    const auto ptr = ctx->builder.CreateLoad(ptr_ty, ptr_addr, "ptr");
    const auto elem_addr = ctx->builder.CreateGEP(ty, ptr, index, "elem_addr");
    const auto elem = ctx->builder.CreateLoad(ty, elem_addr, "elem");

    // Create the "Some" variant.
    const auto undef_opt = llvm::UndefValue::get(opt_ty);
    const auto some_val = ctx->builder.CreateInsertValue(undef_opt, elem, {0}, "some_val");
    const auto some = ctx->builder.CreateInsertValue(some_val, llvm::ConstantInt::getBool(*ctx->context, true), {1}, "some");
    ctx->builder.CreateBr(exit_bb);

    // If out of bounds, return "None".
    ctx->builder.SetInsertPoint(else_bb);
    const auto none_val = llvm::UndefValue::get(opt_ty);
    const auto none = ctx->builder.CreateInsertValue(none_val, llvm::ConstantInt::getBool(*ctx->context, false), {1}, "none");
    ctx->builder.CreateBr(exit_bb);

    // Exit and return the result.
    ctx->builder.SetInsertPoint(exit_bb);
    const auto phi = ctx->builder.CreatePHI(opt_ty, 2, "result");
    phi->addIncoming(some, then_bb);
    phi->addIncoming(none, else_bb);
    ctx->builder.CreateRet(phi);
}


auto spp::codegen::func_impls::std_memops_sizeof(
    analyse::scopes::ScopeManager const *sm,
    asts::FunctionPrototypeAst const *proto,
    LLvmCtx *ctx,
    std::shared_ptr<asts::TypeAst> const &spp_ty) -> void {
    // Calculate the size of the type of object, known at compile time.
    const auto size_ty = llvm_type(*sm->current_scope->get_type_symbol(asts::generate::common_types::usize(0)), ctx);
    const auto size_const = llvm::ConstantInt::get(size_ty, size_of(*sm, spp_ty));

    const auto fn_ty = llvm::FunctionType::get(size_ty, {}, false);
    const auto fn = llvm::Function::Create(
        fn_ty, llvm::Function::ExternalLinkage, mangle::mangle_fun_name(*sm->current_scope, *proto), ctx->module.get());

    // Create the entry basic block.
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);
    ctx->builder.SetInsertPoint(entry_bb);
    ctx->builder.CreateRet(size_const);
}


// auto spp::codegen::func_impls::std_memops_sizeof_value(
//     analyse::scopes::ScopeManager const *sm,
//     asts::FunctionPrototypeAst const *proto,
//     LLvmCtx *ctx)
//     -> void {
//     // Calculate the size of the object, known at runtime, not at compile time.
//     const auto size_ty = sm->current_scope->get_type_symbol(asts::generate::common_types::usize(0))->llvm_info->llvm_type;
//
//     const auto fn_ty = llvm::FunctionType::get(size_ty, {llvm::PointerType::get(*ctx->context, 0)}, false);
//     const auto fn = llvm::Function::Create(
//         fn_ty, llvm::Function::ExternalLinkage, mangle::mangle_fun_name(*sm->current_scope, *proto), ctx->module.get());
//
//     // Create the entry basic block.
//     const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", fn);
//     ctx->builder.SetInsertPoint(entry_bb);
//     // Extract the argument for "self".
//     auto arg_it = fn->args().begin();
//     const auto self = arg_it++;
//
//     // Call llvm.sizeof intrinsic.
//     const auto sizeof_intrinsic = llvm::Intrinsic::getOrInsertDeclaration(
//         ctx->module.get(), llvm::Intrinsic::objectsize, {llvm::PointerType::get(*ctx->context, 0), size_ty});
//     const auto size_val = ctx->builder.CreateCall(sizeof_intrinsic, {self}, "size_val");
//     ctx->builder.CreateRet(size_val);
// }


auto spp::codegen::func_impls::std_intrinsics_add(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAdd);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsics_add_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAdd);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsics_sub(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSub);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsics_sub_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSub);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_mul(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateMul);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_mul_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateMul);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_sdiv(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSDiv);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_sdiv_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSDiv);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_udiv(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateUDiv);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_udiv_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateUDiv);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_srem(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSRem);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_srem_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateSRem);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_urem(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateURem);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_urem_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateURem);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_sneg(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_UN_OP_CLOSURE(CreateNeg);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shl(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateShl);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shl_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateShl);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shr(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLShr);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_shr_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateLShr);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_ior(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateOr);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_ior_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateOr);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_and(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAnd);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_and_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateAnd);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_xor(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateXor);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_bit_xor_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateXor);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_abs(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::abs;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_eq(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpEQ);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_oeq(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFCmpOEQ);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_ne(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpNE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_one(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFCmpONE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_slt(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSLT);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_ult(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpULT);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_olt(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFCmpOLT);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_sle(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSLE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_ule(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpULE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_ole(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFCmpOLE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_sgt(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSGT);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_ugt(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpUGT);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_ogt(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFCmpOGT);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_sge(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpSGE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_uge(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateICmpUGE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_oge(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFCmpOGE);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_smin(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::smin;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_smax(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::smax;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_umin(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::umin;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_umax(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::umax;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fadd(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFAdd);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fadd_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFAdd);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fsub(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFSub);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fsub_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFSub);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fmul(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFMul);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fmul_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFMul);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fdiv(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFDiv);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fdiv_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFDiv);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_frem(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFRem);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_frem_assign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateFRem);
    simple_intrinsic_binop_assign(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fneg(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_UN_OP_CLOSURE(CreateFNeg);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fsqrt(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sqrt;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fpowi(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::powi;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fpowf(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::pow;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fsin(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sin;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fcos(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::cos;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ftan(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::tan;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fasin(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::asin;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_facos(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::acos;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fatan(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::atan;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fatan2(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::atan2;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fsinh(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sinh;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fcosh(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::cosh;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ftanh(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::tanh;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fexp(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::exp;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fexp2(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::exp2;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fexp10(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::exp10;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fln(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::log;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_flog2(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::log2;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_flog10(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::log10;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fabs(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::fabs;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fmax(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::maxnum;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fmin(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::minnum;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fcopysign(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::copysign;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ffloor(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::floor;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fceil(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ceil;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ftrunc(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::trunc;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fround(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::round;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_fbitreverse(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::bitreverse;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ctlz(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ctlz;
    simple_unary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sadd_overflow(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sadd_with_overflow;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_uadd_overflow(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::uadd_with_overflow;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ssub_overflow(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ssub_with_overflow;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_usub_overflow(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::usub_with_overflow;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_smul_overflow(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::smul_with_overflow;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_umul_overflow(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::umul_with_overflow;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sadd_saturating(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sadd_sat;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_uadd_saturating(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::uadd_sat;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ssub_saturating(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ssub_sat;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_usub_saturating(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::usub_sat;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sshl_saturating(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::sshl_sat;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_ushl_saturating(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::ushl_sat;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}


auto spp::codegen::func_impls::std_intrinsic_sadd_wrapping(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNSWAdd);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_uadd_wrapping(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNUWAdd);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_ssub_wrapping(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNSWSub);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_usub_wrapping(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNUWSub);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_smul_wrapping(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNSWMul);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_umul_wrapping(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_BIN_OP_CLOSURE(CreateNUWMul);
    simple_intrinsic_binop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_sitofp(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateSIToFP);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_uitofp(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateUIToFP);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fptrunc(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPTrunc);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_strunc(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateTrunc);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_utrunc(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateTrunc);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_szext(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateSExt);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_uzext(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateZExt);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fpext(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPExt);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fptosi(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPToSI);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fptoui(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    const auto closure = MAKE_CONV_CLOSURE(CreateFPToUI);
    simple_intrinsic_unop(sm, proto, ctx, ty, closure);
}


auto spp::codegen::func_impls::std_intrinsic_fpclass(analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty) -> void {
    constexpr auto intrinsic = llvm::Intrinsic::is_fpclass;
    simple_binary_intrinsic_call(sm, proto, ctx, ty, intrinsic);
}
