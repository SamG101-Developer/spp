module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_call_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


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


auto spp::asts::FunctionCallArgumentAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Delegate to the value.
    const auto llvm_val = val->stage_10_code_gen_2(sm, meta, ctx);
    const auto llvm_type = llvm_val->getType();

    // Handle the convention (to pointer).
    if (conv != nullptr) {
        // If the lhs is symbolic, get the address of the outermost part.
        const auto [sym, _] = sm->current_scope->get_var_symbol_outermost(*meta->postfix_expression_lhs);
        auto llvm_val_ptr = static_cast<llvm::Value*>(nullptr);
        if (sym != nullptr) {
            // Get the alloca for the lhs symbol (the base pointer).
            const auto lhs_alloca = llvm_val;
            llvm_val_ptr = ctx->builder.CreateLoad(llvm_type, lhs_alloca, "load.arg.base_ptr");
        }
        else {
            // Materialize the lhs expression into a temporary.
            const auto temp = ctx->builder.CreateAlloca(llvm_type, nullptr, "temp.arg.borrow");
            ctx->builder.CreateStore(llvm_val, temp);
            llvm_val_ptr = temp;
        }
        return llvm_val_ptr;
    }

    return llvm_val;
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
