#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/loop_condition_boolean_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>


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
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_END;
}


auto spp::asts::LoopConditionBooleanAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the condition expression.
    ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Check the loop condition is boolean.
    const auto cond_type = cond->infer_type(sm, meta);
    const auto target_type = generate::common_types_precompiled::BOOL;
    if (not analyse::utils::type_utils::is_type_boolean(*cond_type, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionNotBooleanError>().with_args(
            *cond, *cond_type, "loop").with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::LoopConditionBooleanAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory state of the condition expression.
    cond->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *cond, *cond, *sm, true, true, false, false, false, false, meta);
}
