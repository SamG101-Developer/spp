module;
#include <spp/analyse/macros.hpp>
#include <spp/macros.hpp>

module spp.asts.inner_scope_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;


SPP_MOD_BEGIN
template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = InnerScopeAst<T>::clone().release()->template to<InnerScopeAst<T>>();
    return std::make_unique<InnerScopeExpressionAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::new_empty()
    -> std::unique_ptr<InnerScopeExpressionAst> {
    return std::make_unique<InnerScopeExpressionAst>(
        nullptr, decltype(InnerScopeExpressionAst::members)(), nullptr);
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve each member of the inner scope.
    sm->move_to_next_scope();
    for (auto const &member : this->members) {
        const auto did_ret = member->template to<RetStatementAst>() != nullptr;
        member->stage_9_comptime_resolution(sm, meta);
        if (did_ret) { break; }
    }

    // Exit the scope.
    sm->move_out_of_current_scope();
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Add all the expressions/statements into the current scope.
    sm->move_to_next_scope();
    // SPP_ASSERT(sm->current_scope == m_scope);

    auto ret_val = static_cast<llvm::Value*>(nullptr);
    for (auto const &member : this->members) {
        ret_val = member->stage_11_code_gen_2(sm, meta, ctx);
    }

    // Exit the scope.
    sm->move_out_of_current_scope();

    if (ret_val and meta->llvm_assignment_target) {
        ctx->builder.CreateStore(ret_val, meta->llvm_assignment_target);
        return ret_val;
    }
    return ret_val;
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // If there are any members, return the last member's inferred type.
    if (not this->members.empty()) {
        auto tm = ScopeManager(sm->global_scope, get_ast_scope());
        return this->members.back()->infer_type(&tm, meta);
    }

    // Otherwise, return the void type.
    return generate::common_types::void_type(pos_start());
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::terminates() const
    -> bool {
    // The inner scope expression only terminates if the last member terminates.
    if (this->members.empty()) { return false; }
    return this->members.back()->terminates();
}


template struct spp::asts::InnerScopeExpressionAst<std::unique_ptr<spp::asts::StatementAst>>;

SPP_MOD_END
