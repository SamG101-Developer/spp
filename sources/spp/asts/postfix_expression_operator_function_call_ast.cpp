module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.overload_utils;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts.utils;
import spp.codegen.llvm_type;
import spp.lex;
import spp.utils.uid;
import genex;
import opex.cast;


spp::asts::PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
    decltype(generic_arg_group) &&generic_arg_group,
    decltype(arg_group) &&arg_group,
    decltype(fold) &&fold) :
    PostfixExpressionOperatorAst(),
    m_overload_info(std::nullopt),
    m_is_async(nullptr),
    m_closure_dummy_arg(nullptr),
    m_is_coro_and_auto_resume(false),
    closure_dummy_proto(nullptr),
    generic_arg_group(std::move(generic_arg_group)),
    arg_group(std::move(arg_group)),
    fold(std::move(fold)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_arg_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->arg_group);
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_start() const
    -> std::size_t {
    return generic_arg_group->pos_start();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_end() const
    -> std::size_t {
    return fold ? fold->pos_end() : arg_group->pos_end();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(
        ast_clone(generic_arg_group),
        ast_clone(arg_group),
        ast_clone(fold));
    ast->closure_dummy_proto = ast_clone(closure_dummy_proto);
    ast->transformed_ast = ast_clone(transformed_ast);
    ast->m_overload_info = m_overload_info;
    ast->m_is_async = m_is_async;
    ast->m_folded_asts = ast_clone_vec(m_folded_asts);
    ast->m_closure_dummy_arg_group = ast_clone(m_closure_dummy_arg_group);
    ast->m_closure_dummy_arg = ast_clone(m_closure_dummy_arg);
    ast->m_is_coro_and_auto_resume = m_is_coro_and_auto_resume;
    return ast;
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(generic_arg_group);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_APPEND(fold);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::handle_function_folding(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> std::vector<std::unique_ptr<PostfixExpressionOperatorFunctionCallAst>> {
    // Populate the list of arguments to fold.
    auto folded_args = std::vector<FunctionCallArgumentAst*>{};
    auto folded_arg_types = std::vector<TypeAst*>{};
    auto folded_tup_lens = std::vector<std::size_t>{};
    auto fold_indexes = std::vector<std::size_t>{};
    for (auto [i, arg] : arg_group->get_all_args() | genex::views::enumerate) {
        auto arg_type = arg->infer_type(sm, meta);
        if (analyse::utils::type_utils::is_type_tuple(*arg_type, *sm->current_scope)) {
            fold_indexes.emplace_back(i);
            folded_args.emplace_back(arg);
            folded_arg_types.emplace_back(arg_type.get());
            folded_tup_lens.emplace_back(arg_type->type_parts().back()->generic_arg_group->args.size());
        }
    }

    // Build the unrolled AST transformations.
    const auto smallest_tuple = genex::min_element(folded_tup_lens);
    auto transformed_asts = std::vector<std::unique_ptr<PostfixExpressionOperatorFunctionCallAst>>{};
    for (auto i = 0uz; i < smallest_tuple; ++i) {
        auto new_arg_group = ast_clone(arg_group);
        for (const auto fold_index : fold_indexes) {
            // Create the postfix access into the tuple.
            auto id = std::make_unique<IdentifierAst>(0, std::to_string(fold_index));
            auto ma = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(id));
            auto pf = std::make_unique<PostfixExpressionAst>(std::move(new_arg_group->args[fold_index]->val), std::move(ma));
            new_arg_group->args[fold_index]->val = std::move(pf);
        }

        auto transformed_ast = ast_clone(this);
        transformed_ast->arg_group = std::move(new_arg_group);
        transformed_ast->fold = nullptr;
        transformed_asts.emplace_back(std::move(transformed_ast));
    }

    // Return the transformed asts.
    return transformed_asts;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Prevent double analysis.
    // Todo: See why this might be happening anyway, and remove this check preferably.
    if (m_overload_info.has_value()) { return; }

    // Analyse the generic arguments and the function call arguments before determining the overload.
    meta->save();
    meta->return_type_overload_resolver_type = nullptr;
    generic_arg_group->stage_7_analyse_semantics(sm, meta);
    arg_group->stage_7_analyse_semantics(sm, meta);
    meta->restore();

    // If we are function folding, create transformed asts.
    if (fold != nullptr) {
        m_folded_asts = handle_function_folding(sm, meta);
        for (auto &&ast : m_folded_asts) { ast->stage_7_analyse_semantics(sm, meta); }
        return;
    }

    // Resolve the overload for this function call.
    auto [overload, is_closure] = analyse::utils::overload_utils::determine_overload(*this, sm, meta);

    // Special case for closures; apply the convention the closure name to ensure is it movable/mutable etc.
    // Todo: move this from overload selection to semantic_analysis?
    if (is_closure) {
        const auto lhs_type = analyse::utils::func_utils::is_target_callable(*meta->postfix_expression_lhs, *sm, meta);
        auto dummy_self_arg = std::make_unique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, ast_clone(meta->postfix_expression_lhs));
        if (analyse::utils::type_utils::symbolic_eq(*lhs_type->without_generics(), *common_types_precompiled::FUN_MUT, *sm->current_scope, *sm->current_scope)) {
            dummy_self_arg->conv = std::make_unique<ConventionMutAst>(nullptr, nullptr);
        }
        else if (analyse::utils::type_utils::symbolic_eq(*lhs_type->without_generics(), *common_types_precompiled::FUN_REF, *sm->current_scope, *sm->current_scope)) {
            dummy_self_arg->conv = std::make_unique<ConventionRefAst>(nullptr);
        }
        m_closure_dummy_arg = std::move(dummy_self_arg);
    }

    // Set the overload to the only pass overload.
    m_overload_info = std::make_tuple(
        std::get<0>(overload),
        std::get<1>(overload),
        std::move(std::get<3>(overload)));
    if (const auto self_param = std::get<1>(*m_overload_info)->param_group->get_self_param()) {
        arg_group->args[0]->conv = ast_clone(self_param->conv);
    }
    arg_group->args = std::move(std::get<2>(std::move(overload))->args); // This doesn't move "overload", just allows ".1" to be moved off of it

    // Check that if we are in a cmp context, that the overload is also cmp.
    raise_if<analyse::errors::SppInvalidComptimeOperationError>(
        meta->enclosing_function_cmp != nullptr and std::get<1>(*m_overload_info)->tok_cmp == nullptr,
        {sm->current_scope}, ERR_ARGS(*this));

    // Special case for GenOnce called as a coroutine => auto move into the "Yield" type.
    if (std::get<1>(*m_overload_info)->tok_fun->token_type == lex::SppTokenType::KW_COR and not meta->prevent_auto_generator_resume) {
        m_is_coro_and_auto_resume = analyse::utils::type_utils::symbolic_eq(
            *common_types_precompiled::GEN_ONCE, *std::get<1>(*m_overload_info)->return_type->without_generics(),
            *sm->current_scope, *std::get<0>(*m_overload_info));
    }
    meta->prevent_auto_generator_resume = false;

    // Copy some properties into the transform (clone arg group for the self arg convention).
    if (transformed_ast) {
        const auto transformed_op = transformed_ast->op->to<PostfixExpressionOperatorFunctionCallAst>();
        transformed_op->arg_group = ast_clone(arg_group);
        transformed_op->m_overload_info = m_overload_info;
        transformed_op->m_is_async = m_is_async;
        transformed_op->m_folded_asts = ast_clone_vec(m_folded_asts);
        transformed_op->m_closure_dummy_arg = ast_clone(m_closure_dummy_arg);
    }
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If a fold is taking place, analyse the folded transformations.
    if (fold != nullptr) {
        for (auto &&ast : m_folded_asts) { ast->stage_8_check_memory(sm, meta); }
        return;
    }

    // If a closure is being called, apply memory rules to the symbolic target.
    if (m_closure_dummy_arg != nullptr) {
        auto closure_args = std::vector<std::unique_ptr<FunctionCallArgumentAst>>();
        closure_args.emplace_back(std::move(m_closure_dummy_arg));
        m_closure_dummy_arg_group = std::make_unique<FunctionCallArgumentGroupAst>(nullptr, std::move(closure_args), nullptr);
        m_closure_dummy_arg_group->stage_7_analyse_semantics(sm, meta);
        m_closure_dummy_arg_group->stage_8_check_memory(sm, meta);
    }

    // Check the argument group, now the old borrows hae been invalidated.
    generic_arg_group->stage_8_check_memory(sm, meta);

    meta->save();
    meta->target_call_function_prototype = std::get<1>(*m_overload_info);
    meta->target_call_was_function_async = m_is_async;
    arg_group->stage_8_check_memory(sm, meta);
    meta->restore();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Todo: For now, don't allow folding in comptime.
    raise_if<analyse::errors::SppCompileTimeConstantError>(
        fold != nullptr,
        {sm->current_scope}, ERR_ARGS(*fold));

    // Get the function prototype and resolve it.
    const auto fn_proto = std::get<1>(*m_overload_info)->non_generic_impl();

    // Create the argument map for the function to use.
    auto args = std::vector<std::pair<std::shared_ptr<IdentifierAst>, std::unique_ptr<ExpressionAst>>>();

    const auto check_self = self_comptime != nullptr;
    if (check_self and not arg_group->get_keyword_args().empty() and arg_group->get_keyword_args().front()->name->val == "self") { // todo: use: .at("self") != nullptr
        self_comptime->stage_9_comptime_resolution(sm, meta);
        args.emplace_back(std::make_unique<IdentifierAst>(0, "self"), std::move(meta->cmp_result));
    }

    for (auto const &arg : arg_group->get_keyword_args()) {
        if (check_self and arg->name->val == "self") { continue; }
        arg->stage_9_comptime_resolution(sm, meta);
        args.emplace_back(arg->name, std::move(meta->cmp_result));
    }
    auto arg_map = decltype(meta->cmp_args)();
    for (auto &&[name, val] : args) {
        arg_map[name] = std::move(val);
    }

    // Resolve the function with the arguments.
    meta->save();
    meta->cmp_args = std::move(arg_map);
    auto tm = ScopeManager(sm->global_scope, fn_proto->get_ast_scope()); // const_cast<analyse::scopes::Scope*>(std::get<0>(*m_overload_info)));
    tm.reset(tm.current_scope->children[0].get());
    fn_proto->impl->stage_9_comptime_resolution(&tm, meta);
    meta->restore();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx) -> llvm::Value* {
    // For folding, generate the code for the folded transformations and combine into single block.
    if (fold != nullptr) {
        const auto merge = InnerScopeExpressionAst<std::unique_ptr<StatementAst>>::new_empty();
        merge->members = m_folded_asts
            | genex::views::transform([&meta](auto &&ast) {
                auto clone_lhs = ast_clone(meta->postfix_expression_lhs);
                auto pf = std::make_unique<PostfixExpressionAst>(std::move(clone_lhs), std::move(ast));
                return std::unique_ptr<StatementAst>(pf.release());
            })
            | genex::to<std::vector>();
        return merge->stage_11_code_gen_2(sm, meta, ctx);
    }

    // For generically converted function prototypes, generate their llvm func once.
    // Todo: Is this even needed?
    if (std::get<1>(*m_overload_info)->get_llvm_func() == nullptr) {
        auto tm = ScopeManager(sm->global_scope, const_cast<analyse::scopes::Scope*>(std::get<0>(*m_overload_info)));
        tm.reset(tm.current_scope);
        std::get<1>(*m_overload_info)->stage_10_code_gen_1(&tm, meta, ctx);
    }

    // SPP_ASSERT(not ctx->builder.GetInsertBlock()->getTerminator());
    const auto uid = spp::utils::generate_uid(this);
    auto llvm_self_arg = static_cast<llvm::Value*>(nullptr);

    if (not arg_group->get_keyword_args().empty() and arg_group->get_keyword_args()[0]->name->val == "self") {
        // Get the type of the left-hand-side expression.
        const auto lhs_type = meta->postfix_expression_lhs->to<PostfixExpressionAst>()->lhs->infer_type(sm, meta);
        const auto lhs_type_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, lhs_type);
        const auto llvm_type = codegen::llvm_type(*lhs_type_sym, ctx);
        SPP_ASSERT(llvm_type != nullptr);

        // If the lhs is non-symbolic, we need to materialize it, and use as the self argument.
        const auto [sym, _] = analyse::utils::scope_utils::get_var_symbol_outermost(
            *sm->current_scope, *meta->postfix_expression_lhs->to<PostfixExpressionAst>()->lhs);
        auto base_ptr = static_cast<llvm::Value*>(nullptr);
        if (sym != nullptr) {
            // Get the alloca for the lhs symbol (the base pointer).
            const auto lhs_alloca = sym->llvm_info->alloca;
            SPP_ASSERT(lhs_alloca != nullptr);
            base_ptr = ctx->builder.CreateLoad(llvm_type, lhs_alloca, "load.member_access.base_ptr" + uid);
        }
        else {
            // Materialize the lhs expression into a temporary.
            const auto lhs_val = meta->postfix_expression_lhs->stage_11_code_gen_2(sm, meta, ctx);
            const auto temp = ctx->builder.CreateAlloca(llvm_type, nullptr, "temp.member_access.lhs" + uid);
            ctx->builder.CreateStore(lhs_val, temp);
            base_ptr = temp;
        }

        // Use the proper pointer to the self arg (sometimes for semantic analysis dummy vars are used).
        llvm_self_arg = ctx->builder.CreateLoad(llvm_type, base_ptr, "load.member_access.self_arg" + uid);
    }

    // Get the llvm function target, and generate the argument values.
    const auto llvm_func = std::get<1>(*m_overload_info)->get_llvm_func()->target;
    SPP_ASSERT(llvm_func != nullptr);
    auto llvm_func_args = arg_group->args
        | genex::views::transform([sm, meta, ctx](auto const &x) { return x->stage_11_code_gen_2(sm, meta, ctx); })
        | genex::to<std::vector>();
    if (llvm_self_arg != nullptr) { llvm_func_args[0] = llvm_self_arg; }

    // Create the call instruction.
    const auto llvm_call = ctx->builder.CreateCall(llvm_func, llvm_func_args, "call" + uid);
    return llvm_call;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // For function folding, collect a tuple of all return types.
    if (not m_folded_asts.empty()) {
        auto folded_return_types = m_folded_asts
            | genex::views::transform([sm, meta](auto &&ast) { return ast->infer_type(sm, meta); })
            | genex::to<std::vector>();
        auto tuple_type = common_types::tuple_type(0, std::move(folded_return_types));
        return tuple_type;
    }

    // Get the function return type from the overload.
    auto ret_type = std::get<1>(*m_overload_info)->return_type;

    // If there is a scope present (non-closure), then fully qualify the return type.
    if (std::get<0>(*m_overload_info) != nullptr) {
        ret_type = analyse::utils::scope_utils::get_type_symbol(*std::get<0>(*m_overload_info), ret_type)->fq_name();
    }

    // For GenOnce coroutines, automatically resume the coroutine and return the "Yield" type.
    if (m_is_coro_and_auto_resume and not meta->prevent_auto_generator_resume) {
        auto [_, yield_type, _] = analyse::utils::type_utils::get_generator_and_yield_type(
            *ret_type, *sm->current_scope, *meta->postfix_expression_lhs, "function call");
        ret_type = yield_type;
    }

    // Return the type.
    return ret_type;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::mark_as_async(
    Ast *async_token)
    -> void {
    m_is_async = async_token;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::target() const
    -> FunctionPrototypeAst* {
    return m_overload_info.has_value() ? std::get<1>(*m_overload_info) : nullptr;
}
