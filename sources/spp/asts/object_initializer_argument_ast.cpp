module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;


spp::asts::ObjectInitializerArgumentAst::ObjectInitializerArgumentAst(
    decltype(name) name,
    decltype(val) &&val) :
    name(std::move(name)),
    val(std::move(val)) {
}


spp::asts::ObjectInitializerArgumentAst::~ObjectInitializerArgumentAst() = default;


auto spp::asts::ObjectInitializerArgumentAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward analysis into the value expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(val.get());
    val->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ObjectInitializerArgumentAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the value expression.
    val->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *val, *this, *sm, true, true, true, true, true, true, meta);
}


auto spp::asts::ObjectInitializerArgumentAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve the value expression.
    val->stage_9_comptime_resolution(sm, meta);
}


auto spp::asts::ObjectInitializerArgumentAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type of the value expression.
    return val->infer_type(sm, meta);
}
