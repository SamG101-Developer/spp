module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.lex.tokens;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::CasePatternVariantDestructureTupleAst::CasePatternVariantDestructureTupleAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}


spp::asts::CasePatternVariantDestructureTupleAst::~CasePatternVariantDestructureTupleAst() = default;


auto spp::asts::CasePatternVariantDestructureTupleAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::CasePatternVariantDestructureTupleAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::CasePatternVariantDestructureTupleAst::clone() const
    -> std::unique_ptr<Ast> {
    auto c = std::make_unique<CasePatternVariantDestructureTupleAst>(
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
    c->m_mapped_let = ast_clone(m_mapped_let);
    return c;
}


spp::asts::CasePatternVariantDestructureTupleAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems, ", ");
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureTupleAst::convert_to_variable(
    CompilerMetaData *meta)
    ->
    std::unique_ptr<LocalVariableAst> {
    // Recursively map the elements to their local variable counterparts.
    auto mapped_elems = elems
        | genex::views::transform([meta](auto &&x) { return x->convert_to_variable(meta); })
        | genex::to<std::vector>();

    // Create the final local variable wrapping, tag it and return it.
    auto var = std::make_unique<LocalVariableDestructureTupleAst>(nullptr, std::move(mapped_elems), nullptr);
    var->mark_from_case_pattern();
    return var;
}


auto spp::asts::CasePatternVariantDestructureTupleAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the new variable from the pattern in the patterns scope.
    auto var = convert_to_variable(meta);
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, ast_clone(meta->case_condition));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);

    // Note there is no nested analysis of "elems", because the "let" statement handles it.
    analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs_dummy_core(
        elems | genex::views::ptr | genex::to<std::vector>(), sm, meta);
}


auto spp::asts::CasePatternVariantDestructureTupleAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checking to the mapped let statement.
    m_mapped_let->stage_8_check_memory(sm, meta);
}


auto spp::asts::CasePatternVariantDestructureTupleAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
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


auto spp::asts::CasePatternVariantDestructureTupleAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
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
