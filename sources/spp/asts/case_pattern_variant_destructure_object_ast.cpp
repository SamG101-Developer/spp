module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.lex.tokens;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import genex;


spp::asts::CasePatternVariantDestructureObjectAst::CasePatternVariantDestructureObjectAst(
    decltype(type) type,
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    type(std::move(type)),
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}


spp::asts::CasePatternVariantDestructureObjectAst::~CasePatternVariantDestructureObjectAst() = default;


auto spp::asts::CasePatternVariantDestructureObjectAst::from_type(
    std::shared_ptr<TypeAst> const &type)
    -> std::unique_ptr<CasePatternVariantDestructureObjectAst> {
    auto empty_elems = std::vector<std::unique_ptr<CasePatternVariantAst>>{};
    return std::make_unique<CasePatternVariantDestructureObjectAst>(type, nullptr, std::move(empty_elems), nullptr);
}


auto spp::asts::CasePatternVariantDestructureObjectAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::CasePatternVariantDestructureObjectAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::CasePatternVariantDestructureObjectAst::clone() const
    -> std::unique_ptr<Ast> {
    auto c = std::make_unique<CasePatternVariantDestructureObjectAst>(
        ast_clone(type),
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
    c->m_mapped_let = ast_clone(m_mapped_let);
    return c;
}


spp::asts::CasePatternVariantDestructureObjectAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems, ", ");
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureObjectAst::convert_to_variable(
    CompilerMetaData *meta)
    -> std::unique_ptr<LocalVariableAst> {
    // Recursively map the elements to their local variable counterparts.
    auto mapped_elems = elems
        | genex::views::transform([meta](auto const &x) { return x->convert_to_variable(meta); })
        | genex::to<std::vector>();

    // Create the final local variable wrapping, tag it and return it.
    auto var = std::make_unique<LocalVariableDestructureObjectAst>(ast_clone(type), nullptr, std::move(mapped_elems), nullptr);
    var->mark_from_case_pattern();
    return var;
}


auto spp::asts::CasePatternVariantDestructureObjectAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // TODO: Move flow typing logic to local variabl mapping
    // TODO: Flow typing doesn't work on nested variant destructuring
    // Analyse the class type (required for flow typing).

    auto conv = ast_clone(type->get_convention());
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(type)->fq_name();
    type = type->with_convention(std::move(conv));

    // Get the condition symbol if it exists.
    m_cond_sym = sm->current_scope->get_var_symbol(ast_clone(meta->case_condition->to<IdentifierAst>()));
    auto cond = meta->case_condition;
    if (m_cond_sym == nullptr) {
        auto cond_type = meta->case_condition->infer_type(sm, meta);

        // Create a variable and let statement for the condition.
        const auto uid = spp::utils::generate_uid(this);
        auto var_name = std::make_shared<IdentifierAst>(pos_start(), uid);
        auto var_ast = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, var_name, nullptr);
        const auto let_ast = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var_ast), cond_type, nullptr, ast_clone(meta->case_condition));

        // TODO: Same logic for array and tuple destructures?
        SPP_DEREF_ALLOW_MOVE_HELPER(let_ast->val) {
            meta->save();
            meta->allow_move_deref = true;
            let_ast->stage_7_analyse_semantics(sm, meta);
            meta->restore();
        }
        else {
            let_ast->stage_7_analyse_semantics(sm, meta);
        }

        // Set the memory information of the symbol based on the type of iteration.
        cond = var_name.get();
        m_cond_sym = sm->current_scope->get_var_symbol(var_name);
    }

    // Flow type the condition symbol if necessary.
    if (analyse::utils::type_utils::is_type_variant(*m_cond_sym->type, *sm->current_scope)) {
        if (not analyse::utils::type_utils::symbolic_eq(*m_cond_sym->type, *type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                .with_args(*meta->case_condition, *m_cond_sym->type, *type, *type)
                .raises_from(sm->current_scope);
        }

        m_flow_sym = std::make_shared<analyse::scopes::VariableSymbol>(*m_cond_sym);
        m_flow_sym->type = type;
        sm->current_scope->add_var_symbol(m_flow_sym);
    }

    // Create the new variable from the pattern in the patterns scope.
    auto var = convert_to_variable(meta);
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, ast_clone(cond));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);

    // Note there is no nested analysis of "elems", because the "let" statement handles it.
    analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs_dummy_core(
        elems | genex::views::ptr | genex::to<std::vector>(), sm, meta);
}


auto spp::asts::CasePatternVariantDestructureObjectAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checking to the mapped let statement.
    m_mapped_let->stage_8_check_memory(sm, meta);
}


auto spp::asts::CasePatternVariantDestructureObjectAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // TODO: Do a non-variant type comparison first.
    // TODO: Do not allow if the condition type is variant.
    // Transform the pattern into comptime values; all need to be true.
    auto comptime_tranforms = analyse::utils::case_utils::create_and_analyse_pattern_eq_comptime(
        elems | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // All must be true for the pattern to match (look for any false).
    const auto all_true = genex::all_of(
        comptime_tranforms,
        [](auto const &x) { return x->template to<BooleanLiteralAst>()->is_true(); });

    // Based on the result, return the corresponding comptime value.
    const auto p = pos_start();
    meta->cmp_result = all_true ? BooleanLiteralAst::True(p) : BooleanLiteralAst::False(p);
}


auto spp::asts::CasePatternVariantDestructureObjectAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Attach the alloca to the potential flow symbol from the outer version of it.
    if (m_flow_sym and m_cond_sym) {
        m_flow_sym->llvm_info->alloca = m_cond_sym->llvm_info->alloca;
    }

    // Generate the "let" statement to introduce all the symbols.
    if (m_mapped_let == nullptr) {
        auto var = convert_to_variable(meta);
        m_mapped_let = std::make_unique<LetStatementInitializedAst>(
            nullptr, std::move(var), nullptr, nullptr, ast_clone(meta->case_condition));
        m_mapped_let->stage_7_analyse_semantics(sm, meta);
    }
    m_mapped_let->stage_11_code_gen_2(sm, meta, ctx);

    // Combine all the generated transforms into a single "AND"ed statement.
    auto llvm_transforms = analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs_llvm(
        elems | genex::views::ptr | genex::to<std::vector>(), sm, meta, ctx);
    const auto combine_func = [&ctx](auto *a, auto *b) { return ctx->builder.CreateAnd(a, b); };
    const auto llvm_master_transform = llvm_transforms.empty()
        ? dynamic_cast<llvm::Value*>(llvm::ConstantInt::getTrue(*ctx->context))
        : genex::fold_left_first(llvm_transforms, std::move(combine_func));

    // Return the combined statement.
    return llvm_master_transform;
}
