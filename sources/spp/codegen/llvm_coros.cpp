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
    -> Pair<llvm::StructType*, llvm::StructType*> {
    //
    using analyse::utils::type_utils::TypeEq;

    // Get the type being yielded.
    const auto [generator_type, yield_type, _] = analyse::utils::type_utils::GetGenAndYieldTypes(
        *coro->ReturnType, scope, *coro->ReturnType, "coroutine prototype");
    const auto send_type = generator_type->TypeParts().Back()->GnArgGroup->TypeAt("Send")->Val;

    // Store the resume function.
    const auto llvm_resume_func_ptr_type = llvm::PointerType::get(*ctx->Context, 0);

    // Define the fields for the coro environment: coro state, current yield location.
    const auto llvm_state_type = llvm::Type::getInt8Ty(*ctx->Context);
    const auto llvm_location_type = llvm::Type::getInt32Ty(*ctx->Context);

    // Next, a tuple of the arguments being bound to this generator.
    auto arg_struct_fields = Vec<llvm::Type*>();
    for (auto const &param_name : coro->FnParamGroup->Params
         | genex::views::transform([](auto &&x) { return x->ExtractNames(); })
         | genex::views::join
         | genex::to<Vec>()) {
        const auto param_sym = scope.GetVarSymbol(param_name);
        const auto param_type_sym = scope.GetTypeSymbol(param_sym->Type);
        arg_struct_fields.EmplaceBack(llvm_type(*param_type_sym, ctx));
    }
    const auto llvm_arg_struct_type = llvm::StructType::create(
        *ctx->Context, arg_struct_fields.ToStdVector(), "gen.args");

    // Yield slot.
    const auto llvm_yield_type = llvm_type(*scope.GetTypeSymbol(yield_type), ctx);

    // For Send != Void, the send slot is also required (dummy otherwise for order).
    const auto llvm_send_type = not TypeEq(*send_type, *asts::generate::common_types_precompiled::VOID, scope, scope)
        ? llvm_type(*scope.GetTypeSymbol(send_type), ctx)
        : llvm::Type::getInt8Ty(*ctx->Context); // Dummy

    // Create the struct type and return it.
    const auto fields = Vec<llvm::Type*>{
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

    const auto coro_env_type = llvm::StructType::create(*ctx->Context, fields.ToStdVector(), "gen.env");
    coro->LlvmCoroEnvType = coro_env_type;
    return MakePair(coro_env_type, llvm_arg_struct_type);
}


auto spp::codegen::create_coro_gen_ctor(
    asts::CoroutinePrototypeAst *coro,
    LLvmCtx *ctx,
    analyse::scopes::Scope const &scope)
    -> std::tuple<llvm::Function*, llvm::Value*, llvm::Type*> {
    const auto uid = spp::utils::Uid(coro);
    const auto [coro_env_type, coro_env_args_type] = create_coro_env_type(coro, ctx, scope);
    if (coro_env_type == nullptr or coro_env_args_type == nullptr) {
        return {nullptr, nullptr, nullptr};
    }

    SPP_ASSERT(coro_env_type != nullptr);

    // Create the gen ctor function - accept, fill, and return the generator environment.
    const auto llvm_func_name = coro->GetLlvmFunc()->Target->getName().str() + ".gen.ctor" + uid;
    const auto llvm_func_type = llvm::FunctionType::get(
        coro_env_type, {}, false);
    const auto llvm_func = llvm::Function::Create(
        llvm_func_type, llvm::Function::InternalLinkage, llvm_func_name, ctx->Module.get());

    // Entry block to construct the generator environment into.
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry" + uid, llvm_func);
    ctx->Builder.SetInsertPoint(entry_bb);

    const auto coro_env_ptr = ctx->Builder.CreateAlloca(
        coro_env_type, nullptr, "coro.env" + uid);

    // Set the state to 0 (READY).
    ctx->Builder.CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), static_cast<std::uint8_t>(CoroutineState::READY)),
        ctx->Builder.CreateStructGEP(coro_env_type, coro_env_ptr, 0, "coro.env.state" + uid));

    // Set the location to 0 (start).
    ctx->Builder.CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 0),
        ctx->Builder.CreateStructGEP(coro_env_type, coro_env_ptr, 1, "coro.env.loc" + uid));

    // Load all the arguments into the args struct.
    const auto load_coro_env = ctx->Builder.CreateLoad(coro_env_type, coro_env_ptr, "coro.env.load" + uid);
    for (auto const &[i, param_name] : coro->FnParamGroup->Params
         | genex::views::transform([](auto &&x) { return x->ExtractNames(); })
         | genex::views::join
         | genex::views::enumerate
         | genex::to<Vec>()) {
        // Get the alloca from the parameter variable.
        const auto puid = spp::utils::Uid(param_name.get());
        const auto param_sym = scope.GetVarSymbol(param_name);
        const auto param_alloca = param_sym->LlvmInfo->Alloca;

        // Load the argument value.
        const auto param_type_sym = scope.GetTypeSymbol(param_sym->Type);
        const auto param_llvm_type = llvm_type(*param_type_sym, ctx);
        const auto param_val = ctx->Builder.CreateLoad(param_llvm_type, param_alloca, "coro.arg.load" + uid + puid);

        // Insert into the args struct.
        const auto gep_arg = ctx->Builder.CreateStructGEP(
            coro_env_args_type, load_coro_env, static_cast<std::uint32_t>(i), "coro.arg.gep" + uid + puid);
        ctx->Builder.CreateStore(param_val, gep_arg);
    }

    // Return the created coroutine environment.
    ctx->Builder.CreateRet(load_coro_env);
    return std::make_tuple(llvm_func, coro_env_ptr, coro_env_args_type);
}


auto spp::codegen::create_coro_res_func(
    asts::CoroutinePrototypeAst const *coro,
    llvm::Type *llvm_arg_struct_type,
    LLvmCtx *ctx,
    analyse::scopes::Scope const &scope)
    -> llvm::Function* {
    // Get the "send" type for the resume signature.
    const auto uid = spp::utils::Uid(coro);
    const auto [generator_type, _, _] = analyse::utils::type_utils::GetGenAndYieldTypes(
        *coro->ReturnType, scope, *coro->ReturnType, "coroutine prototype");
    const auto send_type = generator_type->TypeParts().Back()->GnArgGroup->TypeAt("Send")->Val;
    const auto llvm_send_type = llvm_type(*scope.GetTypeSymbol(send_type), ctx);

    // Create the resume function.
    const auto llvm_func_name = coro->GetLlvmFunc()->Target->getName().str() + ".coro.resume" + uid;
    const auto llvm_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*ctx->Context),
        {llvm::PointerType::get(*ctx->Context, 0), llvm_send_type}, false);
    const auto llvm_func = llvm::Function::Create(
        llvm_func_type, llvm::Function::InternalLinkage, llvm_func_name, ctx->Module.get());

    // Entry block to construct the coroutine body into.
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry" + uid, llvm_func);
    ctx->Builder.SetInsertPoint(entry_bb);

    // Build the argument bindings from the genenv to local variables.
    const auto llvm_coro_env_ptr = llvm_func->getArg(0);
    const auto llvm_coro_env_type = llvm::PointerType::get(*ctx->Context, 0);
    const auto load_coro_env = ctx->Builder.CreateLoad(llvm_coro_env_type, llvm_coro_env_ptr, "coro.env.load" + uid);
    for (auto const &[i, param_name] : coro->FnParamGroup->Params
         | genex::views::transform([](auto &&x) { return x->ExtractNames(); })
         | genex::views::join
         | genex::views::enumerate) {
        // Get the alloca from the parameter variable.
        const auto param_sym = scope.GetVarSymbol(param_name);
        const auto param_type = scope.GetTypeSymbol(param_sym->Type);
        param_sym->LlvmInfo->Alloca = ctx->Builder.CreateAlloca(
            llvm_type(*param_type, ctx), nullptr, "coro.arg.alloca" + uid);

        // Load the argument value from the gen env args struct.
        const auto gep_args = ctx->Builder.CreateStructGEP(llvm_arg_struct_type, load_coro_env, 2, "coro.args.gep" + uid);
        const auto gep_arg = ctx->Builder.CreateStructGEP(llvm_arg_struct_type, gep_args, static_cast<std::uint32_t>(i), "coro.arg.gep" + uid);
        const auto arg_val = ctx->Builder.CreateLoad(llvm_type(*scope.GetTypeSymbol(param_sym->Type), ctx), gep_arg, "coro.arg.load" + uid);

        // Store into the local alloca.
        ctx->Builder.CreateStore(arg_val, param_sym->LlvmInfo->Alloca);
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
    const auto llvm_closure_func_ptr_type = llvm::PointerType::get(*ctx->Context, 0);
    const auto llvm_fut_ptr_type = llvm::PointerType::get(*ctx->Context, 0);

    const auto spawn_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*ctx->Context),
        {llvm_closure_func_ptr_type, llvm_fut_ptr_type}, false);

    const auto internal_spawn_func_name = Str("async_internal_spawn") + mangle::mangle_type_name(fut_type_sym);
    if (const auto func = ctx->Module->getFunction(internal_spawn_func_name); func != nullptr) {
        return func;
    }

    const auto internal_spawn_func = llvm::Function::Create(
        spawn_func_type, llvm::Function::InternalLinkage,
        internal_spawn_func_name, ctx->Module.get());

    internal_spawn_func->addFnAttr(llvm::Attribute::NoUnwind);
    internal_spawn_func->addFnAttr(llvm::Attribute::NoInline);
    return internal_spawn_func;
}
