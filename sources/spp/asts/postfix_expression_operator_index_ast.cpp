module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_index_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


spp::asts::PostfixExpressionOperatorIndexAst::PostfixExpressionOperatorIndexAst(
    std::unique_ptr<TokenAst> tok_l,
    std::unique_ptr<TokenAst> tok_mut,
    std::unique_ptr<ExpressionAst> expr,
    std::unique_ptr<TokenAst> tok_r) :
    m_mapped_func(nullptr),
    tok_l(std::move(tok_l)),
    tok_mut(std::move(tok_mut)),
    expr(std::move(expr)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}


spp::asts::PostfixExpressionOperatorIndexAst::~PostfixExpressionOperatorIndexAst() = default;


auto spp::asts::PostfixExpressionOperatorIndexAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::PostfixExpressionOperatorIndexAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::PostfixExpressionOperatorIndexAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<PostfixExpressionOperatorIndexAst>(
        ast_clone(tok_l),
        ast_clone(tok_mut),
        ast_clone(expr),
        ast_clone(tok_r));
    ast->m_mapped_func = m_mapped_func;
    return ast;
}


spp::asts::PostfixExpressionOperatorIndexAst::operator std::string() const {
    SPP_STRING_START;
    if (m_mapped_func != nullptr) {
        SPP_STRING_APPEND(m_mapped_func->op);
        SPP_STRING_END;
    }
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_APPEND(tok_mut).append(tok_mut ? " " : "");
    SPP_STRING_APPEND(expr);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorIndexAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Already analysed => return early.
    if (m_mapped_func != nullptr) { return; }

    // Determine the left-hand-side type.
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);

    // Ensure the type superimposes the correct indexing variation.
    const auto index_type = tok_mut != nullptr
                                ? generate::common_types_precompiled::INDEX_MUT
                                : generate::common_types_precompiled::INDEX_REF;

    const auto type_sym = sm->current_scope->get_type_symbol(lhs_type);
    auto sup_types = std::vector{lhs_type};
    sup_types.append_range(type_sym->scope->sup_types());

    const auto index_type_candidates = sup_types
        | genex::views::filter([&sm, &index_type](auto const &sup_type) { return analyse::utils::type_utils::symbolic_eq(*sup_type->without_generics(), *index_type, *sm->current_scope, *sm->current_scope); })
        | genex::to<std::vector>();

    if (index_type_candidates.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionNotIndexableError>()
            .with_args(*meta->postfix_expression_lhs, *lhs_type, "runtime indexing")
            .raises_from(sm->current_scope);
    }
    if (index_type_candidates.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionAmbiguousIndexableError>()
            .with_args(*meta->postfix_expression_lhs, *lhs_type, "runtime indexing")
            .raises_from(sm->current_scope);
    }

    // Create the mapped function for the index operator; create the index argument.
    std::unique_ptr<FunctionCallArgumentAst> arg = std::make_unique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, std::move(expr));
    auto arg_group = std::make_unique<FunctionCallArgumentGroupAst>(nullptr, std::vector<decltype(arg)>{}, nullptr);
    arg_group->args.emplace_back(std::move(arg));

    // Field name is either "index_ref" or "index_mut", then call it with the argument group (index).
    auto field_name = std::make_unique<IdentifierAst>(pos_start(), tok_mut != nullptr ? "index_mut" : "index_ref");
    auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
    auto member_access = std::make_unique<PostfixExpressionAst>(ast_clone(meta->postfix_expression_lhs), std::move(field));
    auto func_call = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(arg_group), nullptr);
    m_mapped_func = std::make_shared<PostfixExpressionAst>(std::move(member_access), std::move(func_call));
    m_mapped_func->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::PostfixExpressionOperatorIndexAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the mapped function.
    m_mapped_func->stage_9_comptime_resolution(sm, meta);
}


auto spp::asts::PostfixExpressionOperatorIndexAst::infer_type(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Forward to the mapped function's return type.
    return m_mapped_func->infer_type(sm, meta);
}
