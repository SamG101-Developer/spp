module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts.utils;
import spp.codegen.llvm_type;
import spp.lex;
import spp.utils.strings;
import spp.utils.uid;
import genex;


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(tok_dot) &&tok_dot,
    decltype(name) name) :
    tok_dot(std::move(tok_dot)),
    name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_dot, lex::SppTokenType::TK_DOT, ".");
}


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::~PostfixExpressionOperatorRuntimeMemberAccessAst() = default;


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_start() const
    -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
        ast_clone(tok_dot),
        ast_clone(name));
}


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Prevent types on the left-hand-side of a runtime member access.
    raise_if<analyse::errors::SppMemberAccessStaticOperatorExpectedError>(
        meta->postfix_expression_lhs->to<TypeAst>() != nullptr,
        {sm->current_scope}, ERR_ARGS(*meta->postfix_expression_lhs, *tok_dot));

    // Numeric index access (for tuples).
    if (std::isdigit(name->val[0])) {
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
        const auto lhs_type_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, lhs_type);

        // Check the lhs is a tuple/array (the only indexable types).
        raise_if<analyse::errors::SppMemberAccessNonIndexableError>(
            not analyse::utils::type_utils::is_type_comptime_indexable(*lhs_type, *sm->current_scope),
            {sm->current_scope}, ERR_ARGS(*meta->postfix_expression_lhs, *lhs_type, *tok_dot));

        // Check the index is within the bounds of the tuple/array.
        raise_if<analyse::errors::SppMemberAccessOutOfBoundsError>(
            not analyse::utils::type_utils::is_index_within_type_bound(std::stoul(name->val), *lhs_type, *sm->current_scope),
            {sm->current_scope}, ERR_ARGS(*meta->postfix_expression_lhs, *lhs_type, *tok_dot));
    }

    // Accessing a regular attribute/method on an instance.
    else {
        const auto lhs_as_ident_raw = meta->postfix_expression_lhs->to<IdentifierAst>();
        const auto lhs_as_ident = lhs_as_ident_raw ? std::make_shared<IdentifierAst>(lhs_as_ident_raw->pos_start(), lhs_as_ident_raw->val) : nullptr;
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);

        const auto lhs_ns_sym = analyse::utils::scope_utils::get_ns_symbol(*sm->current_scope, lhs_as_ident);
        const auto lhs_var_sym = analyse::utils::scope_utils::get_var_symbol(*sm->current_scope, lhs_as_ident);
        const auto lhs_type_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, lhs_type);

        // Check the lhs is a variable and not a namespace.
        raise_if<analyse::errors::SppMemberAccessStaticOperatorExpectedError>(
            lhs_var_sym == nullptr and lhs_ns_sym != nullptr, {sm->current_scope},
            ERR_ARGS(*meta->postfix_expression_lhs, *tok_dot));

        // Check the target field exists on the type.
        if (not analyse::utils::scope_utils::has_var_symbol(*lhs_type_sym->scope, name, true)) {
            // At this point, we need to check for the presence of "FwdMut" or "FwdRef" superimpositions, allowing
            // access to their members.
            auto [fwd_ref_type, _] = analyse::utils::type_utils::get_fwd_types(*lhs_type, *sm);
            if (fwd_ref_type != nullptr) {
                const auto inner_type = fwd_ref_type->type_parts().back()->generic_arg_group->type_at("T")->val;
                auto mock_init = std::make_unique<ObjectInitializerAst>(inner_type, nullptr);
                auto mock_access = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, name);
                const auto pf = std::make_unique<PostfixExpressionAst>(std::move(mock_init), std::move(mock_access));
                pf->stage_7_analyse_semantics(sm, meta);
                return;
            }

            // Tpye field was not found on this type, or the forwaring type (includes nested forwarding checks).
            const auto alternatives = analyse::utils::scope_utils::all_var_symbols(*sm->current_scope, true, true)
                | genex::views::transform([](auto const &x) { return x->name->val; })
                | genex::to<std::vector>();

            const auto closest_match = spp::utils::strings::closest_match(name->val, alternatives);
            raise<analyse::errors::SppIdentifierUnknownError>(
                {sm->current_scope}, ERR_ARGS(*this, "instance member", closest_match));
        }

        // Check there is only 1 target field on the type at the highest level.
        if (analyse::utils::scope_utils::get_var_symbol(*lhs_type_sym->scope, name)->type->type_parts().back()->name[0] == '$') {
            return;
        }

        auto scopes_and_syms = std::vector{lhs_type_sym->scope}
            | genex::views::concat(lhs_type_sym->scope->sup_scopes())
            | genex::views::transform([&](auto const &x) { return std::make_pair(x, analyse::utils::scope_utils::get_var_symbol(*x, name)); })
            | genex::views::filter([](auto const &x) { return x.second != nullptr; })
            | genex::views::transform([lhs_type_sym](auto const &x) { return std::make_tuple(lhs_type_sym->scope->depth_difference(x.first), x.first, x.second); })
            | genex::to<std::vector>();

        auto min_depth = genex::min_element(scopes_and_syms
            | genex::views::transform([](auto const &x) { return std::get<0>(x); })
            | genex::to<std::vector>());

        auto closest = scopes_and_syms
            | genex::views::filter([min_depth](auto const &x) { return std::get<0>(x) == min_depth; })
            | genex::views::transform([](auto const &x) { return std::make_pair(std::get<1>(x), std::get<2>(x)); })
            | genex::to<std::vector>();

        if (closest.size() <= 1) { return; }
        raise<analyse::errors::SppAmbiguousMemberAccessError>(
            {closest[0].first, closest[1].first, sm->current_scope},
            ERR_ARGS(*closest[0].second->name, *closest[1].second->name, *name));
    }
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Resolve the left-hand-side expression.
    meta->postfix_expression_lhs->stage_9_comptime_resolution(sm, meta);

    // Handle numeric index access (for tuples).
    if (std::isdigit(name->val[0]) and meta->cmp_result->to<TupleLiteralAst>()) {
        const auto cmp_tup = meta->cmp_result->to<TupleLiteralAst>();
        const auto index = std::stoul(name->val);
        auto cmp_field = ast_clone(cmp_tup->elems[index]);
        meta->cmp_result = std::move(cmp_field);
        return;
    }

    // Handle numeric index access (for arrays).
    if (std::isdigit(name->val[0]) and meta->cmp_result->to<ArrayLiteralExplicitElementsAst>()) {
        const auto cmp_tup = meta->cmp_result->to<ArrayLiteralExplicitElementsAst>();
        const auto index = std::stoul(name->val);
        auto cmp_field = ast_clone(cmp_tup->elems[index]);
        meta->cmp_result = std::move(cmp_field);
        return;
    }

    // Handle normal attribute access (for objects).
    auto cmp_obj = meta->cmp_result->to<ObjectInitializerAst>();
    meta->cmp_result = analyse::utils::cmp_utils::get_attribute_value(cmp_obj, name.get());
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Get the type of the left-hand-side expression.
    const auto uid = spp::utils::generate_uid(this);
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
    const auto lhs_type_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, lhs_type);
    const auto llvm_type = codegen::llvm_type(*lhs_type_sym, ctx);
    SPP_ASSERT(llvm_type != nullptr);

    // If the lhs is symbolic, get the address of the outermost part.
    const auto [sym, _] = analyse::utils::scope_utils::get_var_symbol_outermost(*sm->current_scope, *meta->postfix_expression_lhs);
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

    const auto field_index = analyse::utils::type_utils::get_field_index_in_type(
        *lhs_type, *name, sm);
    const auto llvm_field_index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), field_index);
    const auto llvm_field_ptr = ctx->builder.CreateGEP(llvm_type, base_ptr, llvm_field_index, "member_access.field_ptr" + uid);
    return llvm_field_ptr;
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the type of the left-hand-side expression.
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);

    // Numeric index access (for tuples).
    if (std::isdigit(name->val[0])) {
        const auto elem_type = analyse::utils::type_utils::get_nth_type_of_indexable_type(
            std::stoul(name->val), *lhs_type, *sm->current_scope);
        return elem_type;
    }

    // Get the field symbol and return its type.
    const auto lhs_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, lhs_type);
    const auto field_type = analyse::utils::scope_utils::get_var_symbol(*lhs_sym->scope, name)->type;
    return analyse::utils::scope_utils::get_type_symbol(*lhs_sym->scope, field_type)->fq_name();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::expr_parts() const
    -> std::vector<Ast*> {
    return {name.get()};
}
