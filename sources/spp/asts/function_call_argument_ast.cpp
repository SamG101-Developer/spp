module;
#include <spp/macros.hpp>

module spp.asts.function_call_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.expression_ast;
import spp.asts.type_ast;


spp::asts::FunctionCallArgumentAst::FunctionCallArgumentAst(
    decltype(conv) &&conv,
    decltype(val) &&val,
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag),
    m_self_type(nullptr),
    conv(std::move(conv)),
    val(std::move(val)) {
}


auto spp::asts::FunctionCallArgumentAst::set_self_type(
    std::shared_ptr<TypeAst> self_type)
    -> void {
    // Set the self type to the given type.
    m_self_type = std::move(self_type);
}


auto spp::asts::FunctionCallArgumentAst::get_self_type()
    -> std::shared_ptr<TypeAst> {
    // Get the self type.
    return m_self_type;
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
    return val->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::FunctionCallArgumentAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type from the value expression, unless an explicit "self" type has been given.
    return m_self_type != nullptr
               ? m_self_type
               : val->infer_type(sm, meta)->with_convention(ast_clone(conv));
}
