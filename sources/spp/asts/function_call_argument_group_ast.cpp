#include <algorithm>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/order_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/coroutine_prototype_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/function_call_argument_keyword_ast.hpp>
#include <spp/asts/function_call_argument_positional_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>

#include <genex/actions/pop.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/enumerate.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/map.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/to.hpp>

#include "spp/analyse/utils/mem_utils.hpp"


spp::asts::FunctionCallArgumentGroupAst::FunctionCallArgumentGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(args) &&args,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    args(std::move(args)),
    tok_r(std::move(tok_r)) {
}


spp::asts::FunctionCallArgumentGroupAst::~FunctionCallArgumentGroupAst() = default;


auto spp::asts::FunctionCallArgumentGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::FunctionCallArgumentGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::FunctionCallArgumentGroupAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionCallArgumentGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(args),
        ast_clone(tok_r));
}


spp::asts::FunctionCallArgumentGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(args);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::FunctionCallArgumentGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(args);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::FunctionCallArgumentGroupAst::get_keyword_args() const -> std::vector<FunctionCallArgumentKeywordAst*> {
    return args
        | genex::views::cast.operator()<FunctionCallArgumentKeywordAst>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::ptr_unique
        | genex::views::to<std::vector>();
}


auto spp::asts::FunctionCallArgumentGroupAst::get_positional_args() const -> std::vector<FunctionCallArgumentPositionalAst*> {
    return args
        | genex::views::cast.operator()<FunctionCallArgumentPositionalAst>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::ptr_unique
        | genex::views::to<std::vector>();
}


auto spp::asts::FunctionCallArgumentGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check there are no duplicate argument names.
    const auto arg_names = get_keyword_args()
        | genex::views::map([](auto &&x) { return x->name.get(); })
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not arg_names.empty()) {
        analyse::errors::SppIdentifierDuplicateError(*arg_names[0], *arg_names[1], "keyword function-argument")
            .scopes({sm->current_scope})
            .raise();
    }

    // Check the arguments are in the correct order.
    const auto unordered_args = analyse::utils::order_utils::order_args(args
        | genex::views::cast.operator()<mixins::OrderableAst>()
        | genex::views::ptr_unique
        | genex::views::to<std::vector>());

    if (not unordered_args.empty()) {
        analyse::errors::SppOrderInvalidError(unordered_args[0].first, *unordered_args[0].second, unordered_args[1].first, *unordered_args[1].second)
            .scopes({sm->current_scope})
            .raise();
    }

    // Expand tuple-expansion arguments ("..tuple" => "tuple.0, tuple.1, ...")
    for (auto &&[i, arg] : args | genex::views::ptr_unique | genex::views::enumerate) {
        // Only check position arguments that have ".." tokens.
        const auto pos_arg = ast_cast<FunctionCallArgumentPositionalAst>(arg);
        if (pos_arg == nullptr or pos_arg->tok_unpack == nullptr) { continue; }

        // Check the argument value is a tuple expression.
        auto arg_type = arg->infer_type(sm, meta);
        if (not analyse::utils::type_utils::is_type_tuple(*arg_type, *sm->current_scope)) {
            analyse::errors::SppExpansionOfNonTupleError(*arg->val, *arg_type)
                .scopes({sm->current_scope})
                .raise();
        }

        // Replace the tuple-expansion argument with the expanded arguments.
        args |= genex::actions::pop(i);
        for (auto j = arg_type->type_parts().back()->generic_arg_group->args.size() - 1; j > -1; --j) {
            auto field = std::make_unique<IdentifierAst>(arg->val->pos_start(), std::to_string(i));
            auto new_ast = std::make_unique<PostfixExpressionAst>(
                ast_clone(arg->val),
                std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            auto new_arg = std::make_unique<FunctionCallArgumentPositionalAst>(ast_clone(arg->conv), nullptr, std::move(new_ast));
            args.insert(args.begin() + i, std::move(new_arg));
        }
    }

    // Analyse the arguments
    for (auto &&arg : args) {
        arg->stage_7_analyse_semantics(sm, meta);
        const auto [sym, _] = sm->current_scope->get_var_symbol_outermost(*arg->val);
        if (sym == nullptr) { continue; }
        if (ast_cast<ConventionMutAst>(arg->conv.get()) == nullptr) { continue; }

        // Immutable symbols cannot be mutated.
        if (not sym->is_mutable) {
            analyse::errors::SppInvalidMutationError(*arg->val, *arg->conv, *sym->memory_info->ast_initialization)
                .scopes({sm->current_scope})
                .raise();
        }

        // Immutable borrows, even if their symbol is mutable, cannot be mutated.
        if (sym->memory_info->ast_borrowed and sym->memory_info->is_borrow_ref) {
            analyse::errors::SppInvalidMutationError(*arg->val, *arg->conv, *sym->memory_info->ast_borrowed)
                .scopes({sm->current_scope})
                .raise();
        }
    }
}


auto spp::asts::FunctionCallArgumentGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // If the target is a coroutine, or the target is called as "async", then pins are required.
    const auto is_target_coro = ast_cast<CoroutinePrototypeAst>(meta->target_call_function_prototype) != nullptr;
    const auto pins_required = meta->target_call_was_function_async or is_target_coro;

    // Define the borrow sets to maintain the law of exclusivity.
    auto borrows_ref = std::vector<Ast*>();
    auto borrows_mut = std::vector<Ast*>();

    // Create the pre-existing borrows (from coroutines, async calls) that are already in scope.
    auto preexisting_borrows_ref = std::map<Ast*, std::vector<IdentifierAst*>>();
    auto preexisting_borrows_mut = std::map<Ast*, std::vector<IdentifierAst*>>();

    // Merge the preexisting borrows into the borrow lists.
    for (auto &&arg : args) {
        const auto arg_val = ast_cast<IdentifierAst>(arg->val.get());
        const auto sym = sm->current_scope->get_var_symbol(*arg_val);
        if (sym == nullptr) { continue; }

        for (auto &&[assignment, b, m, _] : sym->memory_info->borrow_refers_to) {
            if (assignment == nullptr) { continue; }
            (m ? borrows_mut : borrows_ref).emplace_back(assignment);
            (m ? preexisting_borrows_mut : preexisting_borrows_ref).try_emplace(assignment, std::vector<Ast*>{}).first->second.emplace_back(assignment);
        }
    }

    for (auto &&arg : args) {
        // Get the outermost part of the argument as a symbol. If the argument is non-symbolic then there is no need to
        // track borrows to it, as it is a temporary value.
        auto [sym, _] = sm->current_scope->get_var_symbol_outermost(*arg->val);
        if (sym == nullptr) { continue; }

        // Ensure the argument isn't moved or partially moved (applies to all conventions). For non-symbolic arguments,
        // nested checking is done via the argument itself.
        arg->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(*arg->val, *arg, sm, true, true, false, false, false, false, meta);

        if (arg->conv == nullptr) {
            // Don't bother rechecking the moves or partial moves, but ensure that attributes aren't being moved off of
            // a borrowed value and that pins are maintained. Mark the move or partial move of the argument. Note the
            // "check_pins_linked=False" because function calls can only imply an inner scope, so it is guaranteed that
            // lifetimes aren't being extended.
            analyse::utils::mem_utils::validate_symbol_memory(*arg->val, *arg, sm, false, false, true, true, false, true, meta);

            // Check the move doesn't overlap with any borrows. This is to ensure that "f(&x, x)" can never happen,
            // because the first argument requires the owned object to outlive the function call, and moving it as the
            // second argument breaks this. Doesn't apply to copyable types.
            if (not sm->current_scope->get_type_symbol(*arg->val->infer_type(sm, meta))->is_copyable()) {
                auto overlaps = genex::views::concat(borrows_ref, borrows_mut)
                    | genex::views::filter([&arg](auto &&x) { return analyse::utils::mem_utils::memory_region_overlap(*x, *arg->val); })
                    | genex::views::to<std::vector>();
                if (not overlaps.empty()) {
                    analyse::errors::SppMemoryOverlapUsageError(*overlaps[0], *arg->val).scopes({sm->current_scope}).raise();
                }
            }
        }

        else if (ast_cast<ConventionRefAst>(arg->conv.get()) != nullptr) {
            // Check the mutable borrow doesn't overlap with any other borrow in the same scope.
            auto overlaps = genex::views::concat(borrows_ref, borrows_mut)
                | genex::views::filter([&arg](auto &&x) { return analyse::utils::mem_utils::memory_region_overlap(*x, *arg->val); })
                | genex::views::to<std::vector>();
            if (not overlaps.empty()) {
                analyse::errors::SppMemoryOverlapUsageError(*overlaps[0], *arg->val).scopes({sm->current_scope}).raise();
            }

            for (auto *existing_assignment : preexisting_borrows_mut[arg->val.get()]) {
                sm->current_scope->get_var_symbol(*existing_assignment)->memory_info->moved_by(*arg->val);
            }
            for (auto *existing_assignment : preexisting_borrows_ref[arg->val.get()]) {
                sm->current_scope->get_var_symbol(*existing_assignment)->memory_info->moved_by(*arg->val);
            }

            // Auto-pin the argument and the assignment target if required.
            if (pins_required) {
                sym->memory_info->ast_pins.emplace_back(arg->val.get());
                sym->memory_info->is_borrow_mut = true;
                sym->memory_info->borrow_refers_to.emplace_back(arg->val.get(), arg.get(), true, sm->current_scope);
                sym->memory_info->borrow_refers_to.emplace_back(meta->assignment_target, arg.get(), true, sm->current_scope);
                sm->current_scope->get_var_symbol(*meta->assignment_target)->memory_info->ast_pins.emplace_back(arg->val.get());
            }

            // Add the mutable borrow to the mutable borrow set.
            borrows_mut.emplace_back(arg->val.get());
        }

        else if (ast_cast<ConventionRefAst>(arg->conv.get()) != nullptr) {
            // Check the immutable borrow doesn't overlap with any other mutable borrow in the same scope.
            auto overlaps = borrows_mut
                | genex::views::filter([&arg](auto &&x) { return analyse::utils::mem_utils::memory_region_overlap(*x, *arg->val); })
                | genex::views::to<std::vector>();
            if (not overlaps.empty()) {
                analyse::errors::SppMemoryOverlapUsageError(*overlaps[0], *arg->val).scopes({sm->current_scope}).raise();
            }

            for (auto *existing_assignment : preexisting_borrows_mut[arg->val.get()]) {
                sm->current_scope->get_var_symbol(*existing_assignment)->memory_info->moved_by(*arg->val);
            }

            // Auto-pin the argument and the assignment target if required.
            if (pins_required) {
                sym->memory_info->ast_pins.emplace_back(arg->val.get());
                sym->memory_info->is_borrow_ref = true;
                sym->memory_info->borrow_refers_to.emplace_back(arg->val.get(), arg.get(), false, sm->current_scope);
                sym->memory_info->borrow_refers_to.emplace_back(meta->assignment_target, arg.get(), false, sm->current_scope);
                sm->current_scope->get_var_symbol(*meta->assignment_target)->memory_info->ast_pins.emplace_back(arg->val.get());
            }

            // Add the mutable borrow to the mutable borrow set.
            borrows_ref.emplace_back(arg->val.get());
        }
    }
}
