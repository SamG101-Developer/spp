module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.asts.utils;
import genex;


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
    codegen::LlvmCtx *ctx)
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
    return common_types::void_type(pos_start());
}


template <typename T>
auto spp::asts::InnerScopeExpressionAst<T>::terminates() const
    -> bool {
    // The inner scope expression only terminates if the last member terminates.
    if (this->members.empty()) { return false; }
    return this->members.back()->terminates();
}


template struct spp::asts::InnerScopeExpressionAst<std::unique_ptr<spp::asts::StatementAst>>;
