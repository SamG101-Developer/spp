module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.asts.meta.compiler_meta_data;
import spp.lex.tokens;
import spp.utils.strings;
import spp.codegen.llvm_type;
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
    if (meta->postfix_expression_lhs->to<TypeAst>() != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessStaticOperatorExpectedError>()
            .with_args(*meta->postfix_expression_lhs, *tok_dot)
            .raises_from(sm->current_scope);
    }

    // Numeric index access (for tuples).
    if (std::isdigit(name->val[0])) {
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(lhs_type);

        // Check the left-hand-side isn't a generic type. Todo: until constraints.
        if (lhs_type_sym->is_generic) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppGenericTypeInvalidUsageError>()
                .with_args(*meta->postfix_expression_lhs, *lhs_type, "member access")
                .raises_from(sm->current_scope);
        }

        // Check the lhs is a tuple/array (the only indexable types).
        if (not analyse::utils::type_utils::is_type_comptime_indexable(*lhs_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessNonIndexableError>()
                .with_args(*meta->postfix_expression_lhs, *lhs_type, *tok_dot)
                .raises_from(sm->current_scope);
        }

        // Check the index is within the bounds of the tuple/array.
        if (not analyse::utils::type_utils::is_index_within_type_bound(std::stoul(name->val), *lhs_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessOutOfBoundsError>()
                .with_args(*meta->postfix_expression_lhs, *lhs_type, *tok_dot)
                .raises_from(sm->current_scope);
        }
    }

    // Accessing a regular attribute/method on an instance.
    else {
        const auto lhs_as_ident_raw = meta->postfix_expression_lhs->to<IdentifierAst>();
        const auto lhs_as_ident = lhs_as_ident_raw ? std::make_shared<IdentifierAst>(lhs_as_ident_raw->pos_start(), lhs_as_ident_raw->val) : nullptr;
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);

        const auto lhs_ns_sym = sm->current_scope->get_ns_symbol(lhs_as_ident);
        const auto lhs_var_sym = sm->current_scope->get_var_symbol(lhs_as_ident);
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(lhs_type);

        // Check the lhs is a variable and not a namespace.
        if (lhs_var_sym == nullptr and lhs_ns_sym != nullptr) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessStaticOperatorExpectedError>()
                .with_args(*meta->postfix_expression_lhs, *tok_dot)
                .raises_from(sm->current_scope);
        }

        // Check the left-hand-side isn't a generic type.
        if (lhs_type_sym->is_generic) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppGenericTypeInvalidUsageError>()
                .with_args(*meta->postfix_expression_lhs, *lhs_type, "member access")
                .raises_from(sm->current_scope);
        }

        // Check the target field exists on the type.
        if (not lhs_type_sym->scope->has_var_symbol(name, true)) {

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
            const auto alternatives = sm->current_scope->all_var_symbols(true, true)
                | genex::views::transform([](auto const &x) { return x->name->val; })
                | genex::to<std::vector>();

            const auto closest_match = spp::utils::strings::closest_match(name->val, alternatives);
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierUnknownError>()
                .with_args(*this, "instance member", closest_match)
                .raises_from(sm->current_scope);
        }

        // Check there is only 1 target field on the type at the highest level.
        if (lhs_type_sym->scope->get_var_symbol(name)->type->type_parts().back()->name[0] == '$') {
            return;
        }

        auto scopes_and_syms = std::vector{lhs_type_sym->scope}
            | genex::views::concat(lhs_type_sym->scope->sup_scopes())
            | genex::views::transform([name=name.get()](auto const &x) { return std::make_pair(x, x->table.var_tbl.get(ast_clone(name))); })
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

        if (closest.size() > 1) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppAmbiguousMemberAccessError>()
                .with_args(*closest[0].second->name, *closest[1].second->name, *name)
                .raises_from(closest[0].first, closest[1].first, sm->current_scope);
        }
    }
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the type of the left-hand-side expression.
    const auto uid = spp::utils::generate_uid(this);
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
    const auto lhs_type_sym = sm->current_scope->get_type_symbol(lhs_type);
    const auto llvm_type = codegen::llvm_type(*lhs_type_sym, ctx);
    SPP_ASSERT(llvm_type != nullptr);

    // If the lhs is symbolic, get the address of the outermost part.
    const auto [sym, _] = sm->current_scope->get_var_symbol_outermost(*meta->postfix_expression_lhs);
    auto base_ptr = static_cast<llvm::Value*>(nullptr);
    if (sym != nullptr) {
        // Get the alloca for the lhs symbol (the base pointer).
        const auto lhs_alloca = sym->llvm_info->alloca;
        SPP_ASSERT(lhs_alloca != nullptr);
        base_ptr = ctx->builder.CreateLoad(llvm_type, lhs_alloca, "load.member_access.base_ptr" + uid);
    }
    else {
        // Materialize the lhs expression into a temporary.
        const auto lhs_val = meta->postfix_expression_lhs->stage_10_code_gen_2(sm, meta, ctx);
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
    const auto lhs_sym = sm->current_scope->get_type_symbol(lhs_type);
    const auto field_type = lhs_sym->scope->get_var_symbol(name)->type;
    return lhs_sym->scope->get_type_symbol(field_type)->fq_name();
}
