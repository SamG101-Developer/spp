module;
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
    ast->annotations | genex::views::for_each([ast=ast.get()](auto const &a) { a->set_ast_ctx(ast); });
    return ast;
}


auto spp::asts::CoroutinePrototypeAst::m_generate_llvm_declaration(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Function* {
    // Do the base functionality, then create the coroutine environment struct type.
    const auto llvm_resume_fn = FunctionPrototypeAst::m_generate_llvm_declaration(sm, meta, ctx);
    m_llvm_resume_fn = llvm_resume_fn;

    // Create the generator constructor, that immediately returns the generator object.
    // Skip base generic functions, as they do not need coroutine generation.
    const auto [_, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
        *return_type, *sm->current_scope, *return_type, "coroutine");
    if (sm->current_scope->get_type_symbol(yield_type)->llvm_info->llvm_type != nullptr) {
        const auto coro_gen_ctor = codegen::create_coro_gen_ctor(this, ctx, *sm->current_scope);
        llvm_func = coro_gen_ctor;
    }

    return llvm_func;
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
