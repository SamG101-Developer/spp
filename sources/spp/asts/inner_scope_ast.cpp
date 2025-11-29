module;
#include <spp/macros.hpp>

module spp.asts.inner_scope_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.class_member_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.statement_ast;
import spp.asts.sup_member_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst() :
    tok_l(nullptr),
    members(),
    tok_r(nullptr) {
}


template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst(
    decltype(tok_l) &&tok_l,
    decltype(members) &&members,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    members(std::move(members)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_CURLY_BRACE, "{");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_CURLY_BRACE, "}");
}


template <typename T>
spp::asts::InnerScopeAst<T>::~InnerScopeAst() = default;


template <typename T>
auto spp::asts::InnerScopeAst<T>::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<InnerScopeAst>(
        ast_clone(tok_l),
        ast_clone_vec(members),
        ast_clone(tok_r));
}


template <typename T>
spp::asts::InnerScopeAst<T>::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(members);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(members);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::new_empty()
    -> std::unique_ptr<InnerScopeAst> {
    return std::make_unique<InnerScopeAst>(
        nullptr, decltype(members)(), nullptr);
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::final_member() const
    -> Ast* {
    return members.empty()
               ? ast_cast<Ast>(tok_r.get())
               : ast_cast<Ast>(members.back().get());
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a scope for the InnerScopeAst node.
    auto scope_name = analyse::scopes::ScopeBlockName("<inner-scope#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    m_scope = sm->current_scope;

    // Analyse the members of the inner scope.
    for (auto const &x : members) { x->stage_7_analyse_semantics(sm, meta); }
    sm->move_out_of_current_scope();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the next scope.
    sm->move_to_next_scope();

    // Check the memory of each member.
    for (auto const &x : members) { x->stage_8_check_memory(sm, meta); }
    auto all_syms = sm->current_scope->all_var_symbols() | genex::to<std::vector>();
    auto inner_syms = sm->current_scope->all_var_symbols(true);

    // If the final expression of the inner scope is being used (ie assigned ot outer variable), then memory check it.
    if (const auto move = meta->assignment_target; not members.empty() and move != nullptr) {
        if (const auto expr_member = ast_cast<ExpressionAst>(final_member())) {
            analyse::utils::mem_utils::validate_symbol_memory(
                *expr_member, *move, *sm, true, true, true, true, true, true, meta);
        }
    }

    sm->move_out_of_current_scope();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Add all the expressions/statements into the current scope.
    sm->move_to_next_scope();
    for (auto *member : members | genex::views::ptr) {
        if (ast_cast<ExpressionAst>(member) != nullptr) {
            member->stage_10_code_gen_2(sm, meta, ctx);
        }
    }

    // Exit the scope.
    sm->move_out_of_current_scope();
    return nullptr;
}


template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::ClassMemberAst>>;
template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::StatementAst>>;
template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::SupMemberAst>>;
