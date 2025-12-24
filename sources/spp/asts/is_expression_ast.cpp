module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.is_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.bin_utils;
import spp.asts.case_expression_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


spp::asts::IsExpressionAst::IsExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    m_mapped_func(nullptr),
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_op, lex::SppTokenType::KW_IS, "is");
}


spp::asts::IsExpressionAst::~IsExpressionAst() = default;


auto spp::asts::IsExpressionAst::pos_start() const
    -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::IsExpressionAst::pos_end() const
    -> std::size_t {
    return rhs ? rhs->pos_end() : m_mapped_func->pos_end();
}


auto spp::asts::IsExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<IsExpressionAst>(
        ast_clone(lhs),
        ast_clone(tok_op),
        ast_clone(rhs));
    ast->m_mapped_func = m_mapped_func;
    return ast;
}


spp::asts::IsExpressionAst::operator std::string() const {
    SPP_STRING_START;
    if (m_mapped_func) {
        SPP_STRING_APPEND(m_mapped_func);
        SPP_STRING_END;
    }
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_APPEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::IsExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    if (m_mapped_func) {
        SPP_PRINT_APPEND(m_mapped_func);
        SPP_PRINT_END;
    }
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}


auto spp::asts::IsExpressionAst::mapped_func() const
    -> std::shared_ptr<CaseExpressionAst> {
    return m_mapped_func;
}


auto spp::asts::IsExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Ensure TypeAst's aren't used for expression for binary operands.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(lhs.get());
    SPP_ENFORCE_EXPRESSION_SUBTYPE(rhs.get());
    m_lhs_as_id = ast_clone(lhs->to<IdentifierAst>());

    // Convert to a "case" destructure and analyse it.
    const auto n = sm->current_scope->children.size();
    m_mapped_func = analyse::utils::bin_utils::convert_is_expr_to_function_call(*this, sm, meta);
    m_mapped_func->stage_7_analyse_semantics(sm, meta);

    // Add the destructure symbols to the current scope.
    // This includes the lhs symbol if it's been flow typed.
    if (not sm->current_scope->name_as_string().starts_with("<inner-scope#")) {
        auto destructure_syms = sm->current_scope->children[n]->children[0]->all_var_symbols(true, true);
        destructure_syms
            | genex::views::for_each([sm](auto &&x) { sm->current_scope->add_var_symbol(x); });
    }
}


auto spp::asts::IsExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the memory checking to the mapped function.
    m_mapped_func->stage_8_check_memory(sm, meta);
}


auto spp::asts::IsExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // If the lhs was an identifier, the "is" causes it to get flow types, so we need to promote the original "alloca"
    // into the flow typed symbol.
    if (m_lhs_as_id) {
        const auto flow_typed_lhs_sym = sm->current_scope->get_var_symbol(m_lhs_as_id, true);
        if (flow_typed_lhs_sym != nullptr) {
            auto original_sym = sm->current_scope->parent->get_var_symbol(m_lhs_as_id);
            original_sym = original_sym ? original_sym : flow_typed_lhs_sym;
            flow_typed_lhs_sym->llvm_info->alloca = original_sym->llvm_info->alloca;
        }
    }

    // Forward the code generation to the mapped function.
    return m_mapped_func->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::IsExpressionAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Always return a boolean type (successful or failed match).
    return generate::common_types::boolean_type(m_mapped_func->pos_start());
}
