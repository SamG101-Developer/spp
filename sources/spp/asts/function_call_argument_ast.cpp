#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionCallArgumentAst::FunctionCallArgumentAst(
    decltype(conv) &&conv,
    decltype(val) &&val,
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag),
    conv(std::move(conv)),
    val(std::move(val)),
    m_self_type(nullptr) {
}


spp::asts::FunctionCallArgumentAst::~FunctionCallArgumentAst() = default;


auto spp::asts::FunctionCallArgumentAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the semantics of the value expression.
    ENFORCE_EXPRESSION_SUBTYPE(val.get());
    val->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::FunctionCallArgumentAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory status of the value expression.
    val->stage_8_check_memory(sm, meta);
}


auto spp::asts::FunctionCallArgumentAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type from the value expression, unless an explicit "self" type has been given.
    return m_self_type != nullptr
        ? m_self_type
        : val->infer_type(sm, meta)->with_convention(ast_clone(conv));
}
