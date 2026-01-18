module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_call_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_materialize;
import spp.utils.uid;


spp::asts::FunctionCallArgumentAst::FunctionCallArgumentAst(
    decltype(conv) &&conv,
    decltype(val) &&val,
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag),
    injected_self_type(nullptr),
    conv(std::move(conv)),
    val(std::move(val)) {
}


auto spp::asts::FunctionCallArgumentAst::set_self_type(
    std::shared_ptr<TypeAst> self_type)
    -> void {
    // Set the self type to the given type.
    injected_self_type = std::move(self_type);
}


auto spp::asts::FunctionCallArgumentAst::get_self_type()
    -> std::shared_ptr<TypeAst> {
    // Get the self type.
    return injected_self_type;
}


auto spp::asts::FunctionCallArgumentAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the semantics of the value expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(val.get());
    val->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::FunctionCallArgumentAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory status of the value expression.
    val->stage_8_check_memory(sm, meta);
}


auto spp::asts::FunctionCallArgumentAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Delegate comptime resolution to the value expression.
    val->stage_9_comptime_resolution(sm, meta);
}


auto spp::asts::FunctionCallArgumentAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Handle the convention (to pointer).
    if (conv != nullptr) {
        // If the lhs is symbolic, get the address of the outermost part.
        const auto uid = spp::utils::generate_uid(this);
        const auto [sym, _] = sm->current_scope->get_var_symbol_outermost(*meta->postfix_expression_lhs);

        if (sym != nullptr) {
            // Get the alloca for the lhs symbol (the base pointer).
            const auto llvm_alloca = sym->llvm_info->alloca;
            SPP_ASSERT(llvm_alloca->getType()->isPointerTy());
            return llvm_alloca;
        }

        // Materialize the lhs expression into a temporary.
        const auto materialized_val = codegen::llvm_materialize(*val, sm, meta, ctx);
        const auto materialized_sym = sm->current_scope->get_var_symbol(ast_clone(materialized_val));

        const auto llvm_alloca = materialized_sym->llvm_info->alloca;
        SPP_ASSERT(llvm_alloca->getType()->isPointerTy());
        return llvm_alloca;
    }

    return val->stage_11_code_gen_2(sm, meta, ctx);
}


auto spp::asts::FunctionCallArgumentAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type from the value expression, unless an explicit "self" type has been given.
    return injected_self_type != nullptr
        ? injected_self_type
        : val->infer_type(sm, meta)->with_convention(ast_clone(conv));
}
