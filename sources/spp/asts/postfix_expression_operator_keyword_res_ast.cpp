module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
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
import spp.codegen.llvm_coros;
import spp.lex.tokens;
import spp.utils.uid;


spp::asts::PostfixExpressionOperatorKeywordResAst::PostfixExpressionOperatorKeywordResAst(
    decltype(tok_dot) &&tok_dot,
    decltype(tok_res) &&tok_res,
    decltype(arg_group) &&arg_group) :
    tok_dot(std::move(tok_dot)),
    tok_res(std::move(tok_res)),
    arg_group(std::move(arg_group)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_dot, lex::SppTokenType::TK_DOT, ".");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_res, lex::SppTokenType::KW_RES, "res");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->arg_group);
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


auto spp::asts::PostfixExpressionOperatorKeywordResAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Already analysed => return early.
    if (m_mapped_func != nullptr) { return; }

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


auto spp::asts::PostfixExpressionOperatorKeywordResAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // TODO
    // The llvm generator environment is the lhs of this postfix expression. (both Gen and Generated are the env, but
    // separate types for analysis). the "resuming" on Generated types simply shiftf the state inside the environment.
    const auto uid = spp::utils::generate_uid(this);
    const auto llvm_gen_env = meta->postfix_expression_lhs->stage_11_code_gen_2(sm, meta, ctx);
    const auto llvm_gen_env_type = llvm_gen_env->getType();

    // Get the resume function pointer (field 0) from the generator environment.
    const auto resume_slot = ctx->builder.CreateStructGEP(llvm_gen_env_type, llvm_gen_env, 0, "gen.resume.fn.slot" + uid);
    const auto llvm_resume_func_ptr = ctx->builder.CreateLoad(
        llvm::PointerType::get(*ctx->context, 0), resume_slot, "gen.resume.fn.ptr" + uid);

    // Convert the send value, if it exists, to the correct LLVM type.
    const auto llvm_send_value = arg_group != nullptr and not arg_group->args.empty()
        ? arg_group->args[0]->stage_11_code_gen_2(sm, meta, ctx)
        : llvm::UndefValue::get(llvm::Type::getVoidTy(*ctx->context));

    // Call the resume function with the generator environment and send value.
    ctx->builder.CreateCall(
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(*ctx->context),
            {llvm::PointerType::get(*ctx->context, 0), llvm_send_value->getType()},
            false),
        llvm_resume_func_ptr,
        {llvm_gen_env, llvm_send_value});

    // Return the generated value (ie wrapped in the generator environment).
    return llvm_gen_env;
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the generator type.
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
    auto [_, yield_type, _] = analyse::utils::type_utils::get_generator_and_yield_type(
        *lhs_type, *sm->current_scope, *meta->postfix_expression_lhs, "resume expression");
    return yield_type;
}
