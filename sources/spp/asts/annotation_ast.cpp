module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.annotation_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.module_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.type_statement_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;


SPP_MOD_BEGIN
spp::asts::AnnotationAst::AnnotationAst(
    decltype(tok_exclamation_mark) &&tok_at_sign,
    decltype(name) &&name,
    decltype(gn_arg_group) &&gn_arg_group,
    decltype(fn_arg_group) &&fn_arg_group) :
    tok_exclamation_mark(std::move(tok_at_sign)),
    name(std::move(name)),
    gn_arg_group(std::move(gn_arg_group)),
    fn_arg_group(std::move(fn_arg_group)) {
}


spp::asts::AnnotationAst::~AnnotationAst() = default;


auto spp::asts::AnnotationAst::pos_start() const
    -> std::size_t {
    return tok_exclamation_mark->pos_start();
}


auto spp::asts::AnnotationAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::AnnotationAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<AnnotationAst>(
        ast_clone(tok_exclamation_mark),
        ast_clone(name),
        ast_clone(gn_arg_group),
        ast_clone(fn_arg_group));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    return ast;
}


spp::asts::AnnotationAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_exclamation_mark);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::AnnotationAst::operator==(
    AnnotationAst const &that) const
    -> bool {
    return *name == *that.name;
}


auto spp::asts::AnnotationAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Default AST processing (sets context).
    Ast::stage_1_pre_process(ctx);
}


auto spp::asts::AnnotationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Default AST processing (sets scope).
    Ast::stage_2_gen_top_level_scopes(sm, meta);
}


auto spp::asts::AnnotationAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Special annotation handling.
    const auto fq_name = sm->current_scope->get_var_symbol_outermost(*name).first->fq_name()->to_string();
    const auto func_ctx = m_ctx->to<FunctionPrototypeAst>();

    // If this is a "!annotation" annotation, mark it.
    if (fq_name == "std::annotation::annotation") {
        raise_if<analyse::errors::SppAnnotationNotAFunctionError>(
            not func_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx));

        func_ctx->mark_as_annotation();
    }

    // If this is a "!compiler_builtin" annotation, mark it.
    if (fq_name == "std::annotations::compiler_builtin") {
        raise_if<analyse::errors::SppAnnotationNotAFunctionError>(
            not func_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx));

        func_ctx->annotation_info()->is_builtin = true;
    }
}


auto spp::asts::AnnotationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Steps to take.
    //  1. Ensure that the target annotation exists and is callable (a function).
    //  2. Ensure that the target annotation is annotated as "!annotation(...)".
    //  3. Ensure that the target annotation's target is valid for the context AST.
    //  4. For compiler_builtin annotations, ensure they have handlers.

    // Convert the target into a function call to ensure it exists.
    auto fn = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(std::move(gn_arg_group), std::move(fn_arg_group), nullptr);
    const auto pf = std::make_unique<PostfixExpressionAst>(ast_clone(name), std::move(fn));
    pf->stage_7_analyse_semantics(sm, meta);

    // Check the target function is an annotation (via "!annotation").
    const auto overload = pf->op->to<PostfixExpressionOperatorFunctionCallAst>()->target();
    const auto annotation_info = overload ? overload->annotation_info() : nullptr;
    raise_if<analyse::errors::SppAnnotationTargetNotAnAnnotationError>(
        not annotation_info,
        {m_scope}, ERR_ARGS(*this, *overload));

    // Annotation target checks.
    const auto outer_mod_ctx = m_ctx->get_ast_ctx()->to<ModulePrototypeAst>();

    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<ClassPrototypeAst>() and ~(annotation_info->ctx & analyse::utils::annotation_utils::AnnotationInfo::CLASS_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<FunctionPrototypeAst>() and outer_mod_ctx and ~(annotation_info->ctx & analyse::utils::annotation_utils::AnnotationInfo::FUNCTION_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<FunctionPrototypeAst>() and not outer_mod_ctx and ~(annotation_info->ctx & analyse::utils::annotation_utils::AnnotationInfo::METHOD_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<TypeStatementAst>() and ~(annotation_info->ctx & analyse::utils::annotation_utils::AnnotationInfo::TYPE_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<CmpStatementAst>() and ~(annotation_info->ctx & analyse::utils::annotation_utils::AnnotationInfo::CMP_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    // Handle builtin annotations.
    if (annotation_info->is_builtin) {
        const auto fq_name = sm->current_scope->get_var_symbol_outermost(*name).first->fq_name()->to_string();
        if (fq_name == "std::annotations::public") {
            const auto vis_ctx = m_ctx->to<mixins::VisibilityEnabledAst>();
            SPP_ASSERT(vis_ctx != nullptr);
            vis_ctx->visibility = std::make_pair(utils::Visibility::PUBLIC, annotation_info->definition);
        }
        else if (fq_name == "std::annotations::protected") {
            const auto vis_ctx = m_ctx->to<mixins::VisibilityEnabledAst>();
            SPP_ASSERT(vis_ctx != nullptr);
            vis_ctx->visibility = std::make_pair(utils::Visibility::PROTECTED, annotation_info->definition);
        }
        else if (fq_name == "std::annotations::private") {
            const auto vis_ctx = m_ctx->to<mixins::VisibilityEnabledAst>();
            SPP_ASSERT(vis_ctx != nullptr);
            vis_ctx->visibility = std::make_pair(utils::Visibility::PRIVATE, annotation_info->definition);
        }
        else if (fq_name == "std::annotations::virtual_method") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->virtual_annotation = annotation_info->definition;
        }
        else if (fq_name == "std::annotations::abstract_method") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->abstract_annotation = annotation_info->definition;
        }
        else if (fq_name == "std::annotations::ffi") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->ffi_annotation = annotation_info->definition;
        }
        else if (fq_name == "std::llvm::inline") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->inline_annotation = annotation_info->definition;
        }
        else if (fq_name == "std::llvm::always_inline") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->inline_annotation = annotation_info->definition;
        }
        else if (fq_name == "std::llvm::noinline") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->inline_annotation = annotation_info->definition;
        }
        else if (fq_name == "std::llvm::hot") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->temperature_annotation = annotation_info->definition;
        }
        else if (fq_name == "std::llvm::cold") {
            const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
            SPP_ASSERT(fun_ctx != nullptr);
            fun_ctx->temperature_annotation = annotation_info->definition;
        }

        // TODO: "extends", "variant_default"

        raise<analyse::errors::SppInternalCompilerError>(
            {sm->current_scope}, ERR_ARGS(*this, "Unknown builtin annoation"));
    }
}

SPP_MOD_END
