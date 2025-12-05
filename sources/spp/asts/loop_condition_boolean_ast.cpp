module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_condition_boolean_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.generate.common_types_precompiled;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.asts.type_ast;


spp::asts::LoopConditionBooleanAst::LoopConditionBooleanAst(
    decltype(cond) &&cond) :
    LoopConditionAst(),
    cond(std::move(cond)) {
}


spp::asts::LoopConditionBooleanAst::~LoopConditionBooleanAst() = default;


auto spp::asts::LoopConditionBooleanAst::pos_start() const
    -> std::size_t {
    return cond->pos_start();
}


auto spp::asts::LoopConditionBooleanAst::pos_end() const
    -> std::size_t {
    return cond->pos_end();
}


auto spp::asts::LoopConditionBooleanAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopConditionBooleanAst>(ast_clone(cond));
}


spp::asts::LoopConditionBooleanAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(cond);
    SPP_STRING_END;
}


auto spp::asts::LoopConditionBooleanAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_END;
}


auto spp::asts::LoopConditionBooleanAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the condition expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Check the loop condition is boolean.
    const auto cond_type = cond->infer_type(sm, meta);
    const auto target_type = generate::common_types_precompiled::BOOL;
    if (not analyse::utils::type_utils::is_type_boolean(*cond_type, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionNotBooleanError>()
            .with_args(*cond, *cond_type, "loop")
            .raises_from(sm->current_scope);
    }
}


auto spp::asts::LoopConditionBooleanAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the condition expression.
    cond->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *cond, *cond, *sm, true, true, false, false, false, false, meta);
}
