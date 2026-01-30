module;
#include <spp/macros.hpp>

module spp.codegen.llvm_coros;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.utils.uid;
import llvm;
import opex.cast;
import genex;
import std;


auto spp::codegen::create_coro_env_type(
    asts::CoroutinePrototypeAst *coro,
    LLvmCtx *ctx,
    analyse::scopes::Scope const &scope)
    -> std::pair<llvm::StructType*, llvm::StructType*> {
    // Get the type being yielded.
    const auto [generator_type, yield_type, _] = analyse::utils::type_utils::get_generator_and_yield_type(
        *coro->return_type, scope, *coro->return_type, "coroutine prototype");
    const auto send_type = generator_type->type_parts().back()->generic_arg_group->type_at("Send")->val;

    // Store the resume function.
    const auto llvm_resume_func_ptr_type = llvm::PointerType::get(*ctx->context, 0);

    // Define the fields for the coro environment: coro state, current yield location.
    const auto llvm_state_type = llvm::Type::getInt8Ty(*ctx->context);
    const auto llvm_location_type = llvm::Type::getInt32Ty(*ctx->context);

    // Next, a tuple of the arguments being bound to this generator.
    auto arg_struct_fields = std::vector<llvm::Type*>();
    for (const auto &param_name : coro->param_group->params
         | genex::views::transform([](auto &&x) { return x->extract_names(); })
         | genex::views::join
         | genex::to<std::vector>()) {
        const auto param_sym = scope.get_var_symbol(param_name);
        const auto param_type_sym = scope.get_type_symbol(param_sym->type);
        arg_struct_fields.emplace_back(llvm_type(*param_type_sym, ctx));
    }
    const auto llvm_arg_struct_type = llvm::StructType::create(
        *ctx->context, arg_struct_fields, "gen.args");

    // Yield slot.
    const auto llvm_yield_type = llvm_type(*scope.get_type_symbol(yield_type), ctx);

    // For Send != Void, the send slot is also required (dummy otherwise for order).
    const auto llvm_send_type = not analyse::utils::type_utils::symbolic_eq(*send_type, *asts::generate::common_types_precompiled::VOID, scope, scope)
                                    ? llvm_type(*scope.get_type_symbol(send_type), ctx)
                                    : llvm::Type::getInt8Ty(*ctx->context); // Dummy

    // Create the struct type and return it.
    const auto fields = std::vector<llvm::Type*>{
        llvm_resume_func_ptr_type,
        llvm_state_type,
        llvm_location_type,
        llvm_arg_struct_type,
        llvm_yield_type,
        llvm_send_type,
    };

    if (llvm_yield_type == nullptr or llvm_send_type == nullptr) {
        return {nullptr, nullptr};
    }

    const auto coro_env_type = llvm::StructType::create(*ctx->context, std::move(fields), "gen.env");
    coro->llvm_coro_env_type = coro_env_type;
    return {coro_env_type, llvm_arg_struct_type};
}


auto spp::codegen::create_coro_gen_ctor(
    asts::CoroutinePrototypeAst *coro,
    LLvmCtx *ctx,
    analyse::scopes::Scope const &scope)
    -> std::tuple<llvm::Function*, llvm::Value*, llvm::Type*> {
    const auto uid = spp::utils::generate_uid(coro);
    const auto [coro_env_type, coro_env_args_type] = create_coro_env_type(coro, ctx, scope);
    if (coro_env_type == nullptr or coro_env_args_type == nullptr) {
        return {nullptr, nullptr, nullptr};
    }

    SPP_ASSERT(coro_env_type != nullptr);

    // Create the gen ctor function - accept, fill, and return the generator environment.
    const auto llvm_func_name = coro->llvm_func->getName().str() + ".gen.ctor" + uid;
    const auto llvm_func_type = llvm::FunctionType::get(
        coro_env_type, {}, false);
    const auto llvm_func = llvm::Function::Create(
        llvm_func_type, llvm::Function::InternalLinkage, llvm_func_name, ctx->llvm_module.get());

    // Entry block to construct the generator environment into.
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry" + uid, llvm_func);
    ctx->builder.SetInsertPoint(entry_bb);

    const auto coro_env_ptr = ctx->builder.CreateAlloca(
        coro_env_type, nullptr, "coro.env" + uid);

    // Set the state to 0 (READY).
    ctx->builder.CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), static_cast<std::uint8_t>(CoroutineState::READY)),
        ctx->builder.CreateStructGEP(coro_env_type, coro_env_ptr, 0, "coro.env.state" + uid));

    // Set the location to 0 (start).
    ctx->builder.CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), 0),
        ctx->builder.CreateStructGEP(coro_env_type, coro_env_ptr, 1, "coro.env.loc" + uid));

    // Load all the arguments into the args struct.
    const auto load_coro_env = ctx->builder.CreateLoad(coro_env_type, coro_env_ptr, "coro.env.load" + uid);
    for (const auto &[i, param_name] : coro->param_group->params
         | genex::views::transform([](auto &&x) { return x->extract_names(); })
         | genex::views::join
         | genex::views::enumerate
         | genex::to<std::vector>()) {
        // Get the alloca from the parameter variable.
        const auto puid = spp::utils::generate_uid(param_name.get());
        const auto param_sym = scope.get_var_symbol(param_name);
        const auto param_alloca = param_sym->llvm_info->alloca;

        // Load the argument value.
        const auto param_type_sym = scope.get_type_symbol(param_sym->type);
        const auto param_llvm_type = llvm_type(*param_type_sym, ctx);
        const auto param_val = ctx->builder.CreateLoad(param_llvm_type, param_alloca, "coro.arg.load" + uid + puid);

        // Insert into the args struct.
        const auto gep_arg = ctx->builder.CreateStructGEP(
            coro_env_args_type, load_coro_env, static_cast<std::uint32_t>(i), "coro.arg.gep" + uid + puid);
        ctx->builder.CreateStore(param_val, gep_arg);
    }

    // Return the created coroutine environment.
    ctx->builder.CreateRet(load_coro_env);
    return {llvm_func, coro_env_ptr, coro_env_args_type};
}


auto spp::codegen::create_coro_res_func(
    asts::CoroutinePrototypeAst const *coro,
    llvm::Type *llvm_arg_struct_type,
    LLvmCtx *ctx,
    analyse::scopes::Scope const &scope)
    -> llvm::Function* {
    // Get the "send" type for the resume signature.
    const auto uid = spp::utils::generate_uid(coro);
    const auto [generator_type, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
        *coro->return_type, scope, *coro->return_type, "coroutine prototype");
    const auto send_type = generator_type->type_parts().back()->generic_arg_group->type_at("Send")->val;
    const auto llvm_send_type = llvm_type(*scope.get_type_symbol(send_type), ctx);

    // Create the resume function.
    const auto llvm_func_name = coro->llvm_func->getName().str() + ".coro.resume" + uid;
    const auto llvm_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*ctx->context),
        {llvm::PointerType::get(*ctx->context, 0), llvm_send_type}, false);
    const auto llvm_func = llvm::Function::Create(
        llvm_func_type, llvm::Function::InternalLinkage, llvm_func_name, ctx->llvm_module.get());

    // Entry block to construct the coroutine body into.
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry" + uid, llvm_func);
    ctx->builder.SetInsertPoint(entry_bb);

    // Build the argument bindings from the genenv to local variables.
    const auto llvm_coro_env_ptr = llvm_func->getArg(0);
    const auto llvm_coro_env_type = llvm::PointerType::get(*ctx->context, 0);
    const auto load_coro_env = ctx->builder.CreateLoad(llvm_coro_env_type, llvm_coro_env_ptr, "coro.env.load" + uid);
    for (const auto &[i, param_name] : coro->param_group->params
         | genex::views::transform([](auto &&x) { return x->extract_names(); })
         | genex::views::join
         | genex::views::enumerate) {
        // Get the alloca from the parameter variable.
        const auto param_sym = scope.get_var_symbol(param_name);
        const auto param_type = scope.get_type_symbol(param_sym->type);
        param_sym->llvm_info->alloca = ctx->builder.CreateAlloca(
            llvm_type(*param_type, ctx), nullptr, "coro.arg.alloca" + uid);

        // Load the argument value from the gen env args struct.
        const auto gep_args = ctx->builder.CreateStructGEP(llvm_arg_struct_type, load_coro_env, 2, "coro.args.gep" + uid);
        const auto gep_arg = ctx->builder.CreateStructGEP(llvm_arg_struct_type, gep_args, static_cast<std::uint32_t>(i), "coro.arg.gep" + uid);
        const auto arg_val = ctx->builder.CreateLoad(llvm_type(*scope.get_type_symbol(param_sym->type), ctx), gep_arg, "coro.arg.load" + uid);

        // Store into the local alloca.
        ctx->builder.CreateStore(arg_val, param_sym->llvm_info->alloca);
    }

    // The switch header block will be injected from outside of this function.
    llvm_func->addFnAttr(llvm::Attribute::NoUnwind);
    llvm_func->addFnAttr(llvm::Attribute::NoInline);
    return llvm_func;
}


auto spp::codegen::create_async_spawn_func(
    LLvmCtx *ctx,
    analyse::scopes::TypeSymbol const &fut_type_sym) -> llvm::Function* {
    // Create the async spawn function.
    const auto llvm_closure_func_ptr_type = llvm::PointerType::get(*ctx->context, 0);
    const auto llvm_fut_ptr_type = llvm::PointerType::get(*ctx->context, 0);

    const auto spawn_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*ctx->context),
        {llvm_closure_func_ptr_type, llvm_fut_ptr_type}, false);

    const auto internal_spawn_func_name = std::string("async_internal_spawn") + mangle::mangle_type_name(fut_type_sym);
    if (const auto func = ctx->llvm_module->getFunction(internal_spawn_func_name); func != nullptr) {
        return func;
    }

    const auto internal_spawn_func = llvm::Function::Create(
        spawn_func_type, llvm::Function::InternalLinkage,
        internal_spawn_func_name, ctx->llvm_module.get());

    internal_spawn_func->addFnAttr(llvm::Attribute::NoUnwind);
    internal_spawn_func->addFnAttr(llvm::Attribute::NoInline);
    return internal_spawn_func;
}
