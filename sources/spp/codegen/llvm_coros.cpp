module;
#include <spp/macros.hpp>

module spp.codegen.llvm_coros;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.coroutine_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import llvm;
import std;


auto spp::codegen::create_coro_environment(
    asts::CoroutinePrototypeAst const *coro,
    LLvmCtx *ctx,
    analyse::scopes::Scope const &scope) -> llvm::StructType* {
    // Get the type being yielded.
    const auto [generator_type, yield_type, _, _, _, error_type] = analyse::utils::type_utils::get_generator_and_yield_type(
        *coro->return_type, scope, *coro->return_type, "coroutine prototype");
    const auto send_type = generator_type->type_parts().back()->generic_arg_group->type_at("Send")->val;

    // Define the fields for the coro environment: coro state, tag and yield slot.
    auto fields = std::vector<llvm::Type*>();
    fields.emplace_back(llvm::Type::getInt32Ty(ctx->context));
    fields.emplace_back(llvm::Type::getInt8Ty(ctx->context));
    fields.emplace_back(scope.get_type_symbol(yield_type)->llvm_info->llvm_type);

    // For GenRes, the error slot is also requires.
    if (analyse::utils::type_utils::symbolic_eq(*coro->return_type->without_generics(), *asts::generate::common_types_precompiled::GEN_RES, scope, scope)) {
        fields.emplace_back(scope.get_type_symbol(error_type)->llvm_info->llvm_type);
    }

    // For Send != Void, the send slot is also required.
    if (not analyse::utils::type_utils::symbolic_eq(*send_type, *asts::generate::common_types_precompiled::VOID, scope, scope)) {
        fields.emplace_back(scope.get_type_symbol(send_type)->llvm_info->llvm_type);
    }

    // Create the struct type and return it.
    const auto struct_name = "coro_env#" + std::to_string(coro->pos_start());
    const auto struct_type = llvm::StructType::create(ctx->context, std::move(fields), struct_name);
    return struct_type;
}


auto spp::codegen::create_coro_gen_ctor(
    asts::CoroutinePrototypeAst const *coro,
    LLvmCtx *ctx,
    analyse::scopes::Scope const &scope) -> llvm::Function* {
    // Create the coroutine environment for the generator's constructor.
    const auto coro_env_type = create_coro_environment(coro, ctx, scope);

    // This function creates a coroutine generator constructor for immediate return.
    const auto func_name = coro->llvm_func->getName().str() + "#ctor";

    const auto llvm_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx->context), {llvm::PointerType::get(ctx->context, 0)}, false);
    const auto llvm_func = llvm::Function::Create(
        llvm_func_type, llvm::Function::InternalLinkage, func_name, ctx->module.get());
    llvm_func->addParamAttr(0, llvm::Attribute::StructRet);

    // Body
    const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", llvm_func);
    ctx->builder.SetInsertPoint(entry_bb);

    const auto gen_out = llvm_func->getArg(0);
    const auto state_ptr = ctx->builder.CreateStructGEP(coro_env_type, gen_out, 0);
    const auto state_initial = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx->context), static_cast<std::uint32_t>(CoroutineState::VARIABLE));

    SPP_ASSERT(state_initial != nullptr and state_ptr != nullptr);
    ctx->builder.CreateStore(state_initial, state_ptr);
    ctx->builder.CreateRetVoid();

    return llvm_func;
}
