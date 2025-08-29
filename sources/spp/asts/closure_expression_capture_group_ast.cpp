#include <algorithm>

#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/closure_expression_capture_ast.hpp>
#include <spp/asts/closure_expression_capture_group_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/local_variable_single_identifier_alias_ast.hpp>
#include <spp/asts/object_initializer_ast.hpp>
#include <spp/asts/object_initializer_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/views/filter.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/ptr.hpp>


spp::asts::ClosureExpressionCaptureGroupAst::ClosureExpressionCaptureGroupAst(
    decltype(tok_caps) &&tok_caps,
    decltype(captures) &&captures) :
    tok_caps(std::move(tok_caps)),
    captures(std::move(captures)) {
}


spp::asts::ClosureExpressionCaptureGroupAst::~ClosureExpressionCaptureGroupAst() = default;


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_start() const -> std::size_t {
    return tok_caps->pos_start();
}


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_end() const -> std::size_t {
    return captures.back()->pos_end();
}


auto spp::asts::ClosureExpressionCaptureGroupAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ClosureExpressionCaptureGroupAst>(
        ast_clone(tok_caps),
        ast_clone_vec(captures));
}


spp::asts::ClosureExpressionCaptureGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_caps);
    SPP_STRING_EXTEND(captures);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionCaptureGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_caps);
    SPP_PRINT_EXTEND(captures);
    SPP_PRINT_END;
}


auto spp::asts::ClosureExpressionCaptureGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Add the capture variables after analysis, otherwise their symbol checks refer to the new captures, not the
    // original asts from the argument group analysis.
    for (auto &&cap : captures) {
        // Create a "let" statement to insert the symbol into the current scope.
        auto cap_val = ast_cast<IdentifierAst>(ast_clone(cap));
        auto var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(cap_val), nullptr);
        auto var_type = cap->val->infer_type(sm, meta);
        auto let_val = std::make_unique<ObjectInitializerAst>(std::move(var_type), nullptr);
        const auto let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, std::move(let_val));
        let->stage_7_analyse_semantics(sm, meta);

        // Apply the borrow to the symbol.
        const auto sym = sm->current_scope->get_var_symbol(ast_cast<IdentifierAst>(*let->val));
        sym->memory_info->ast_borrowed = cap->conv.get();
        sym->memory_info->is_borrow_mut = cap->conv->tag == ConventionAst::ConventionTag::MUT;
        sym->memory_info->is_borrow_ref = cap->conv->tag == ConventionAst::ConventionTag::MUT;
        sym->type = sym->type->with_convention(ast_clone(cap->conv));
    }
}


auto spp::asts::ClosureExpressionCaptureGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Pin the lambda symbol if it is assigned to a variable and has borrowed captures.
    if (meta->assignment_target != nullptr) {
        captures
            | genex::views::ptr
            | genex::views::filter([](auto &&x) { return x->conv != nullptr; })
            | genex::views::for_each([&](auto &&x) { meta->current_lambda_outer_scope->get_var_symbol(*meta->assignment_target)->memory_info->ast_pins.emplace_back(x->val.get()); });
    }

    // Pin any values that have been captured by the closure as borrows.
    for (auto &&cap : captures) {
        if (cap->conv != nullptr) {
            const auto cap_val = ast_cast<IdentifierAst>(cap->val.get());
            const auto cap_sym = sm->current_scope->get_var_symbol(*cap_val);
            cap_sym->memory_info->ast_pins.emplace_back(cap->val.get());
        }
    }
}
