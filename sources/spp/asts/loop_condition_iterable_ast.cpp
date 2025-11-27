module;
#include <genex/views/for_each.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.loop_condition_iterable_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.let_statement_uninitialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;


spp::asts::LoopConditionIterableAst::LoopConditionIterableAst(
    decltype(var) &&var,
    decltype(tok_in) &&tok_in,
    decltype(iterable) &&iterable) :
    LoopConditionAst(),
    var(std::move(var)),
    tok_in(std::move(tok_in)),
    iterable(std::move(iterable)) {
}


spp::asts::LoopConditionIterableAst::~LoopConditionIterableAst() = default;


auto spp::asts::LoopConditionIterableAst::pos_start() const
    -> std::size_t {
    return var->pos_start();
}


auto spp::asts::LoopConditionIterableAst::pos_end() const
    -> std::size_t {
    return iterable->pos_end();
}


auto spp::asts::LoopConditionIterableAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopConditionIterableAst>(
        ast_clone(var),
        ast_clone(tok_in),
        ast_clone(iterable));
}


spp::asts::LoopConditionIterableAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var).append(" ");
    SPP_STRING_APPEND(tok_in).append(" ");
    SPP_STRING_APPEND(iterable);
    SPP_STRING_END;
}


auto spp::asts::LoopConditionIterableAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var).append(" ");
    SPP_PRINT_APPEND(tok_in).append(" ");
    SPP_PRINT_APPEND(iterable);
    SPP_PRINT_END;
}


auto spp::asts::LoopConditionIterableAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the iterable.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(iterable.get());
    iterable->stage_7_analyse_semantics(sm, meta);

    // Get the generator and yielded type from the iterable.
    // Todo: Change to only allow Gen, not GenOpt/GenRes.
    const auto iterable_type = iterable->infer_type(sm, meta);
    auto [gen_type, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
        *iterable_type, *sm, *iterable, "loop condition");

    // Create a "let" statement to introduce the loop variable into the scope.
    const auto let_ast = std::make_unique<LetStatementUninitializedAst>(nullptr, std::move(var), nullptr, yield_type);
    let_ast->stage_7_analyse_semantics(sm, meta);
    var = std::move(let_ast->var);

    // Set the memory information of the symbol based on the type of iteration.
    var->extract_names()
        | genex::views::transform([sm](auto const &x) { return sm->current_scope->get_var_symbol(x); })
        | genex::views::for_each([this, yield_type, sm](auto const &x) {
            const auto conv = yield_type->get_convention();
            x->memory_info->initialized_by(*this, sm->current_scope);
            x->memory_info->ast_borrowed = conv != nullptr ? std::tuple(this, sm->current_scope) : std::tuple(nullptr, nullptr);
        });
}


auto spp::asts::LoopConditionIterableAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the variable.
    if (not meta->loop_double_check_active) {
        iterable->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(
            *iterable, *iterable, *sm, true, true, false, false, false, false, meta);
    }

    // Re-initialize for the double loop analysis.
    var->extract_names()
        | genex::views::transform([sm](auto &&x) { return sm->current_scope->get_var_symbol(x); })
        | genex::views::for_each([this, sm](auto &&x) { x->memory_info->initialized_by(*this, sm->current_scope); });
}
