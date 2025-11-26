module;
#include <genex/to_container.hpp>
#include <genex/views/enumerate.hpp>
#include <genex/views/intersperse.hpp>
#include <genex/views/join.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_array_ast;
import spp.lex.tokens;
import spp.asts.ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_array_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;


spp::asts::CasePatternVariantDestructureArrayAst::CasePatternVariantDestructureArrayAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}


spp::asts::CasePatternVariantDestructureArrayAst::~CasePatternVariantDestructureArrayAst() = default;


auto spp::asts::CasePatternVariantDestructureArrayAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::CasePatternVariantDestructureArrayAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::CasePatternVariantDestructureArrayAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantDestructureArrayAst>(
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::CasePatternVariantDestructureArrayAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureArrayAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantDestructureArrayAst::convert_to_variable(
    meta::CompilerMetaData *meta)
    -> std::unique_ptr<LocalVariableAst> {
    // Recursively map the elements to their local variable counterparts.
    auto mapped_elems = elems
        | genex::views::transform([meta](auto &&x) { return x->convert_to_variable(meta); })
        | genex::to<std::vector>();

    // Create the final local variable wrapping, tag it and return it.
    auto var = std::make_unique<LocalVariableDestructureArrayAst>(
        nullptr, std::move(mapped_elems), nullptr);
    var->m_from_pattern = true;
    return var;
}


auto spp::asts::CasePatternVariantDestructureArrayAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Create the new variable from the pattern in the patterns scope.
    auto var = convert_to_variable(meta);
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(
        nullptr, std::move(var), nullptr, nullptr, ast_clone(meta->case_condition));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantDestructureArrayAst::stage_8_check_memory(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward memory checking to the mapped let statement.
    m_mapped_let->stage_8_check_memory(sm, meta);
}


auto spp::asts::CasePatternVariantDestructureArrayAst::stage_10_code_gen_2(
    ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the "let" statement to introduce all the symbols.
    m_mapped_let->stage_10_code_gen_2(sm, meta, ctx);

    // Create a "master" statement that will be "AND"ed with all the literal checks.
    auto master_stmt = dynamic_cast<llvm::Value*>(llvm::ConstantInt::getTrue(ctx->context));

    // Iterate over each element in the destructuring pattern.
    for (auto const &[i, part] : elems | genex::views::ptr | genex::views::enumerate) {
        // For literals, generate the equality checks.
        if (const auto literal_part = ast_cast<CasePatternVariantLiteralAst>(part); literal_part != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = std::make_unique<IdentifierAst>(0, std::to_string(i));
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = std::make_unique<PostfixExpressionAst>(ast_clone(meta->case_condition), std::move(field));

            // Turn the "literal part" into a function argument.
            auto eq_arg_conv = std::make_unique<ConventionRefAst>(nullptr);
            auto eq_arg_val = ast_cast<ExpressionAst>(ast_clone(literal_part->literal.get()));
            auto eq_arg = std::make_unique<FunctionCallArgumentPositionalAst>(std::move(eq_arg_conv), nullptr, std::move(eq_arg_val));

            // Create the ".eq" part.
            auto eq_field_name = std::make_unique<IdentifierAst>(0, "eq");
            auto eq_field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(eq_field_name));
            auto eq_pf_expr = std::make_unique<PostfixExpressionAst>(std::move(pf_expr), std::move(eq_field));

            // Make the ".eq" part callable, as ".eq()" (no arguments right now)
            auto eq_call = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
            const auto eq_call_expr = std::make_unique<PostfixExpressionAst>(std::move(eq_pf_expr), std::move(eq_call));
            eq_call->arg_group->args.emplace_back(std::move(eq_arg));

            // Generate the equality check.
            const auto llvm_call = eq_call_expr->stage_10_code_gen_2(sm, meta, ctx);
            master_stmt = ctx->builder.CreateAnd(master_stmt, llvm_call);
        }

        // For nested objects (array, tuple, object)
        else if (dynamic_cast<CasePatternVariantDestructureArrayAst*>(part) != nullptr or
            dynamic_cast<CasePatternVariantDestructureTupleAst*>(part) != nullptr or
            dynamic_cast<CasePatternVariantDestructureObjectAst*>(part) != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = std::make_unique<IdentifierAst>(0, std::to_string(i));
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = std::make_unique<PostfixExpressionAst>(ast_clone(meta->case_condition), std::move(field));

            // Update the "meta->cond" with the "pf_expr", and analyse against the inner part.
            meta->save();
            meta->case_condition = pf_expr.get();

            // Combine the result.
            const auto llvm_call = part->stage_10_code_gen_2(sm, meta, ctx);
            master_stmt = ctx->builder.CreateAnd(master_stmt, llvm_call);
            meta->restore();
        }
    }

    // Return the combined statement.
    return master_stmt;
}
