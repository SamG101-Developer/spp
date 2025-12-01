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
import spp.asts.generic_parameter_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

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
    ast->m_abstract_annotation = m_abstract_annotation;
    ast->m_virtual_annotation = m_virtual_annotation;
    ast->m_temperature_annotation = m_temperature_annotation;
    ast->m_no_impl_annotation = m_no_impl_annotation;
    ast->m_inline_annotation = m_inline_annotation;
    ast->m_visibility = m_visibility;
    ast->m_llvm_coro_frame = m_llvm_coro_frame;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto const &a) { a->m_ctx = ast; });
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
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppCoroutineInvalidReturnTypeError>().with_args(
            *this, *return_type).with_scopes({sm->current_scope}).raise();
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
    // Use the default FunctionPrototypeAst then use coroutine intrinsics ("id", "begin", "end", "destroy")
    const auto func = FunctionPrototypeAst::stage_10_code_gen_2(sm, meta, ctx);

    // Use the coro.id intrinsic.
    auto coro_id = ctx->builder.CreateCall(
        llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::coro_id),
        {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx->context), 0),
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm::Type::getVoidTy(ctx->context))),
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm::Type::getVoidTy(ctx->context))),
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm::Type::getVoidTy(ctx->context))),
        });

    // Use the coro.begin intrinsic.
    auto coro_begin = ctx->builder.CreateCall(
        llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), llvm::Intrinsic::coro_begin),
        {
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm::Type::getVoidTy(ctx->context))),
            coro_id,
        });

    // Store the information into the coroutine context.
    m_llvm_coro_frame = std::make_unique<codegen::LlvmCoroFrame>(coro_id, coro_begin);
    return func;
}
