#include <algorithm>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/utils/bin_utils.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/is_expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>

#include <genex/views/for_each.hpp>


spp::asts::IsExpressionAst::IsExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    m_mapped_func(nullptr),
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::IsExpressionAst::~IsExpressionAst() = default;


auto spp::asts::IsExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::IsExpressionAst::pos_end() const -> std::size_t {
    return rhs->pos_end();
}


auto spp::asts::IsExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<IsExpressionAst>(
        ast_clone(lhs),
        ast_clone(tok_op),
        ast_clone(rhs));
}


spp::asts::IsExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_APPEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::IsExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}


auto spp::asts::IsExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Ensure TypeAst's aren't used for expression for binary operands.
    ENFORCE_EXPRESSION_SUBTYPE(lhs.get());
    ENFORCE_EXPRESSION_SUBTYPE(rhs.get());

    // Convert to a "case" destructure and analyse it.
    const auto n = sm->current_scope->children.size();
    m_mapped_func = analyse::utils::bin_utils::convert_is_expr_to_function_call(*this);
    m_mapped_func->stage_7_analyse_semantics(sm, meta);

    // Add the destructure symbols to the current scope.
    auto destructure_syms = sm->current_scope->children[n]->children[0]->all_var_symbols(true, true);
    destructure_syms | genex::views::for_each([sm](auto &&x) {
        sm->current_scope->add_var_symbol(std::shared_ptr<analyse::scopes::VariableSymbol>(x));
    });
}


auto spp::asts::IsExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward the memory checking to the mapped function.
    m_mapped_func->stage_8_check_memory(sm, meta);
}


auto spp::asts::IsExpressionAst::infer_type(
    ScopeManager *,
    mixins::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Always return a boolean type (successful or failed match).
    return generate::common_types::boolean_type(pos_start());
}
