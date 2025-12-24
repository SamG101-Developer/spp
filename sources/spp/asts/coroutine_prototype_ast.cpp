module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.coroutine_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.identifier_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_type;

import genex;
import llvm;


auto spp::asts::CoroutinePrototypeAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<CoroutinePrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_fun),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(param_group),
        ast_clone(tok_arrow),
        ast_clone(return_type),
        ast_clone(impl));
    ast->orig_name = ast_clone(orig_name);
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->abstract_annotation = abstract_annotation;
    ast->virtual_annotation = virtual_annotation;
    ast->temperature_annotation = temperature_annotation;
    ast->no_impl_annotation = no_impl_annotation;
    ast->inline_annotation = inline_annotation;
    ast->visibility = visibility;
    ast->llvm_func = llvm_func;
    ast->llvm_coro_yield_slot = llvm_coro_yield_slot;
    ast->llvm_gen_env = llvm_gen_env;
    ast->m_llvm_resume_fn = m_llvm_resume_fn;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto const &a) { a->set_ast_ctx(ast); });
    return ast;
}


auto spp::asts::CoroutinePrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::stage_7_analyse_semantics(sm, meta);
    const auto ret_type_sym = sm->current_scope->get_type_symbol(return_type);

    // Update the meta information for enclosing function information.
    meta->save();
    meta->enclosing_function_flavour = this->tok_fun.get();
    meta->enclosing_function_ret_type.emplace_back(ret_type_sym->fq_name());
    meta->enclosing_function_scope = sm->current_scope;
    impl->stage_7_analyse_semantics(sm, meta);

    // Check the return type superimposes the generator type.
    auto temp = std::vector<std::shared_ptr<TypeAst>>();
    temp.emplace_back(ret_type_sym->fq_name()->without_generics());
    auto superimposed_types = ret_type_sym->scope->sup_types()
        | genex::views::concat(std::move(temp))
        | genex::views::transform([](auto &&x) { return x->without_generics(); })
        | genex::to<std::vector>();

    if (genex::none_of(superimposed_types, [sm](auto &&x) { return analyse::utils::type_utils::is_type_generator(*x->without_generics(), *sm->current_scope); })) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppCoroutineInvalidReturnTypeError>()
            .with_args(*this, *return_type)
            .raises_from(sm->current_scope);
    }

    // Analyse the semantics of the function body, and move out the scope.
    sm->move_out_of_current_scope();
    meta->restore(true);
    meta->loop_return_types->clear();
}


auto spp::asts::CoroutinePrototypeAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move into the coroutine scope.
    sm->move_to_next_scope();
    // SPP_ASSERT(sm->current_scope == m_scope);

    // Create the coroutine contructor function.
    const auto [llvm_coro_ctor, llvm_gen_env, llem_gen_env_args_type] = codegen::create_coro_gen_ctor(this, ctx, *sm->current_scope);
    if (llvm_gen_env != nullptr) {
        const auto llvm_coro_resume_func = codegen::create_coro_res_func(this, llem_gen_env_args_type, ctx, *sm->current_scope);
        m_llvm_resume_fn = llvm_coro_resume_func; // Save for interaction with ".res()" calls.
        this->llvm_gen_env = llvm_gen_env; // Save for interaction with ".res()" calls.

        // Load the resume function into the 0th field of the coroutine environment.
        ctx->builder.CreateStore(
            ctx->builder.CreateBitCast(llvm_coro_resume_func, llvm::PointerType::get(*ctx->context, 0)),
            ctx->builder.CreateStructGEP(llvm_coro_ctor->getReturnType(), llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::RES_FN)));

        // Entry block into the resume function.
        const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", llvm_coro_resume_func);
        ctx->builder.SetInsertPoint(entry_bb);

        const auto ret_type_sym = sm->current_scope->get_type_symbol(return_type);
        meta->save();
        meta->enclosing_function_flavour = this->tok_fun.get();
        meta->enclosing_function_scope = sm->current_scope;
        meta->enclosing_function_ret_type.emplace_back(ret_type_sym->fq_name());
        impl->stage_10_code_gen_2(sm, meta, ctx);
        meta->restore();

        // Reset to the start of the resume function to build the switch.
        ctx->builder.SetInsertPoint(entry_bb);

        // Create the "switch" header block, mapping location values to labels.
        const auto number_of_yields = static_cast<std::uint32_t>(ctx->yield_continuations.size());
        const auto switch_bb = llvm::BasicBlock::Create(*ctx->context, "coro.switch", llvm_coro_resume_func);

        // Switch on the value loaded from the coroutine environment's location field.
        const auto loc_field = ctx->builder.CreateStructGEP(llvm::PointerType::get(*ctx->context, 0), llvm_coro_resume_func->getArg(0), 1);
        const auto loc_value = ctx->builder.CreateLoad(llvm::Type::getInt32Ty(*ctx->context), loc_field);
        const auto switch_inst = ctx->builder.CreateSwitch(loc_value, switch_bb, number_of_yields + 1);

        // Case for "0" => start of the coroutine.
        const auto start_bb = llvm::BasicBlock::Create(*ctx->context, "coro.start", llvm_coro_resume_func);
        switch_inst->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), 0), start_bb);
        ctx->builder.SetInsertPoint(start_bb);

        // Loop up the yield counter, generating a "case" and "jump" for each yield point.
        for (std::size_t i = 0; i < number_of_yields; ++i) {
            const auto target_block = ctx->yield_continuations[i];
            switch_inst->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), i + 1), target_block);
        }

        // Reset the yield continuations for future coroutines.
        ctx->yield_continuations.clear();
    }
    else {
        // Generic base function so not generating for it.
        // Manual scope skipping.
        const auto final_scope = sm->current_scope->final_child_scope();
        while (sm->current_scope != final_scope) {
            sm->move_to_next_scope(false);
        }
    }
    sm->move_out_of_current_scope();

    if (llvm_gen_env == nullptr) {
        // Analyse to make a new scope in the correct place.
        for (auto &&[_, generic_impl] : m_generic_substitutions) {
            auto tm = ScopeManager(sm->global_scope, m_scope->parent);
            tm.reset(tm.current_scope);
            generic_impl->stage_10_code_gen_2(&tm, meta, ctx);
        }
    }

    return llvm_coro_ctor;
}
