module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.fold_expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::PostfixExpressionOperatorKeywordResAst::PostfixExpressionOperatorKeywordResAst(
    decltype(tok_dot) &&tok_dot,
    decltype(tok_res) &&tok_res,
    decltype(arg_group) &&arg_group) :
    tok_dot(std::move(tok_dot)),
    tok_res(std::move(tok_res)),
    arg_group(std::move(arg_group)) {
}


spp::asts::PostfixExpressionOperatorKeywordResAst::~PostfixExpressionOperatorKeywordResAst() = default;


auto spp::asts::PostfixExpressionOperatorKeywordResAst::pos_start() const
    -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::pos_end() const
    -> std::size_t {
    return arg_group ? arg_group->pos_end() : tok_res->pos_end();
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<PostfixExpressionOperatorKeywordResAst>(
        ast_clone(tok_dot),
        ast_clone(tok_res),
        ast_clone(arg_group));
    ast->m_mapped_func = m_mapped_func;
    return ast;
}


spp::asts::PostfixExpressionOperatorKeywordResAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(tok_res);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(tok_res);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the left-hand-side is a generator type (for specific errors).
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
    analyse::utils::type_utils::get_generator_and_yield_type(
        *lhs_type, *sm->current_scope, *meta->postfix_expression_lhs, "resume expression");

    // Check the argument (send value) is valid, by passing it into the ".send" function call.
    auto send = std::make_unique<IdentifierAst>(pos_start(), "send");
    auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(send));
    auto member_access = std::make_unique<PostfixExpressionAst>(ast_clone(meta->postfix_expression_lhs), std::move(field));
    auto func_call = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(arg_group), nullptr);
    m_mapped_func = std::make_unique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));
    m_mapped_func->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the memory check to the mapped function, which will check the arguments, and the function call.
    m_mapped_func->stage_8_check_memory(sm, meta);
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Forward the generation to the "send" function, which in turn runs the internal "resume" (llvm).
    return m_mapped_func->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the generator type.
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
    auto [gen_type, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
        *lhs_type, *sm->current_scope, *meta->postfix_expression_lhs, "resume expression");

    // Convert the type Gen[Yield] => Generated[Yield]
    if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN, *gen_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
        auto generated_type = generate::common_types::generated_type(pos_start(), std::move(yield_type));
        generated_type->stage_7_analyse_semantics(sm, meta);
        return generated_type;
    }

    // Convert the type GenOpt[Yield] => GeneratedOpt[Yield]
    if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN_OPT, *gen_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
        auto generated_type = generate::common_types::generated_opt_type(pos_start(), std::move(yield_type));
        generated_type->stage_7_analyse_semantics(sm, meta);
        return generated_type;
    }

    // Convert the type GenRes[Yield, Err] => GeneratedRes[Yield, Err]
    if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN_RES, *gen_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
        auto generated_type = generate::common_types::generated_res_type(pos_start(), std::move(yield_type), gen_type->type_parts().back()->generic_arg_group->type_at("Err")->val);
        generated_type->stage_7_analyse_semantics(sm, meta);
        return generated_type;
    }

    // Semantic analysis prevents non-generator types from being used with the ".res" operator, so this should never be
    // reached.
    std::unreachable();
}
