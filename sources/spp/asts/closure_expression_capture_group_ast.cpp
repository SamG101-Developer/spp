module;
#include <genex/to_container.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/intersperse.hpp>
#include <genex/views/join.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.closure_expression_capture_group_ast;
import spp.analyse.utils.mem_utils;
import spp.asts.closure_expression_capture_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.object_initializer_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.lex.tokens;


spp::asts::ClosureExpressionCaptureGroupAst::ClosureExpressionCaptureGroupAst(
    decltype(tok_caps) &&tok_caps,
    decltype(captures) &&captures) :
    tok_caps(std::move(tok_caps)),
    captures(std::move(captures)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_caps, lex::SppTokenType::KW_CAPS, "caps");
}


spp::asts::ClosureExpressionCaptureGroupAst::~ClosureExpressionCaptureGroupAst() = default;


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_start() const
    -> std::size_t {
    return tok_caps->pos_start();
}


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_end() const
    -> std::size_t {
    return captures.back()->pos_end();
}


auto spp::asts::ClosureExpressionCaptureGroupAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ClosureExpressionCaptureGroupAst>(
        ast_clone(tok_caps),
        ast_clone_vec(captures));
}


spp::asts::ClosureExpressionCaptureGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_caps).append(" ");
    SPP_STRING_EXTEND(captures);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionCaptureGroupAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_caps).append(" ");
    SPP_PRINT_EXTEND(captures);
    SPP_PRINT_END;
}


auto spp::asts::ClosureExpressionCaptureGroupAst::new_empty()
    -> std::unique_ptr<ClosureExpressionCaptureGroupAst> {
    return std::make_unique<ClosureExpressionCaptureGroupAst>(
        nullptr, decltype(captures)());
}


auto spp::asts::ClosureExpressionCaptureGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Add the capture variables after analysis, otherwise their symbol checks refer to the new captures, not the
    // original asts from the argument group analysis.
    for (auto &&cap : captures) {
        // Create a "let" statement to insert the symbol into the current scope.
        auto cap_val = ast_cast<IdentifierAst>(ast_clone(cap->val));
        auto var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(cap_val), nullptr);
        auto var_type = cap->val->infer_type(sm, meta);
        auto let_val = std::make_unique<ObjectInitializerAst>(std::move(var_type), nullptr);
        const auto let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, std::move(let_val));
        let->stage_7_analyse_semantics(sm, meta);

        // Apply the borrow to the symbol.
        const auto sym = sm->current_scope->get_var_symbol(ast_cast<IdentifierAst>(ast_clone(cap->val)));
        const auto conv = cap->conv.get();
        sym->memory_info->ast_borrowed = {conv, sm->current_scope};
        sym->type = sym->type->with_convention(ast_clone(cap->conv));
    }
}


auto spp::asts::ClosureExpressionCaptureGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {

    // Any borrowed captures need pinning and marking as extended borrows.
    const auto [ass_sym, _] = meta->current_lambda_outer_scope->get_var_symbol_outermost(*meta->assignment_target);
    for (auto const &cap : captures) {
        if (cap->conv != nullptr) {
            // Mark the pins on the capture and the target.
            const auto cap_val = ast_cast<IdentifierAst>(cap->val.get());
            const auto cap_sym = sm->current_scope->get_var_symbol(ast_clone(cap_val));
            if (ass_sym != nullptr) { ass_sym->memory_info->ast_pins.emplace_back(cap->val.get()); }
            if (cap_sym != nullptr) { cap_sym->memory_info->ast_pins.emplace_back(cap->val.get()); }

            // Mark the extended borrow. TODO
            // auto is_mut = *cap->conv == ConventionAst::ConventionTag::MUT;
            // cap_sym->memory_info->extended_borrows.emplace_back(
            //     cap->val.get(), is_mut, meta->current_lambda_outer_scope);
        }
    }
}
