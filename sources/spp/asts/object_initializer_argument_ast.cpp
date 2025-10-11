#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/object_initializer_argument_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::ObjectInitializerArgumentAst::ObjectInitializerArgumentAst(
    decltype(name) &&name,
    decltype(val) &&val) :
    name(std::move(name)),
    val(std::move(val)) {
}


auto spp::asts::ObjectInitializerArgumentAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward analysis into the value expression.
    ENFORCE_EXPRESSION_SUBTYPE(val.get());
    val->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ObjectInitializerArgumentAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory of the value expression.
    val->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*val, *this, *sm, true, true, true, true, true, true, meta);
}


auto spp::asts::ObjectInitializerArgumentAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type of the value expression.
    return val->infer_type(sm, meta);
}
