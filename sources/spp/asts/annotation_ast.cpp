module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

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
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.module_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.parse.parser_spp;
import spp.lex.lexer;


SPP_MOD_BEGIN
spp::asts::AnnotationAst::AnnotationAst(
    decltype(tok_exclamation_mark) &&tok_exclamation_mark,
    decltype(name) &&name,
    decltype(gn_arg_group) &&gn_arg_group,
    decltype(fn_arg_group) &&fn_arg_group) :
    tok_exclamation_mark(std::move(tok_exclamation_mark)),
    name(std::move(name)),
    gn_arg_group(std::move(gn_arg_group)),
    fn_arg_group(std::move(fn_arg_group)),
    m_target(nullptr) {
    // Default the two optional argument groups.
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->gn_arg_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->fn_arg_group);
}


spp::asts::AnnotationAst::~AnnotationAst() = default;


auto spp::asts::AnnotationAst::pos_start() const
    -> std::size_t {
    // Start at the exclamation mark.
    return tok_exclamation_mark->pos_start();
}


auto spp::asts::AnnotationAst::pos_end() const
    -> std::size_t {
    // Only span to the name's end character.
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
    ast->m_target = m_target;
    return ast;
}


spp::asts::AnnotationAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_exclamation_mark);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(gn_arg_group);
    SPP_STRING_APPEND(fn_arg_group);
    SPP_STRING_END;
}


auto spp::asts::AnnotationAst::operator==(
    AnnotationAst const &that) const
    -> bool {
    // Annotation equality is based on the name.
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


auto spp::asts::AnnotationAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Special annotation handling.
    const auto sym = sm->current_scope->get_var_symbol_outermost(*name).first;
    if (sym == nullptr) { return; }

    const auto fq_name = sym->fq_name()->to_string();
    const auto func_ctx = m_ctx->to<FunctionPrototypeAst>();

    // If this is a "!annotation" annotation, mark it.
    if (fq_name == "std::annotations::annotation") {
        raise_if<analyse::errors::SppAnnotationTargetNotACmpFunctionError>(
            not(func_ctx and func_ctx->tok_cmp), {m_scope},
            ERR_ARGS(*this, *m_ctx));
        func_ctx->mark_as_annotation();
        func_ctx->annotation_info()->definition = this;
    }

    // If this is a "!compiler_builtin" annotation, mark it.
    // if (fq_name == "std::annotations::compiler_builtin") {
    //     raise_if<analyse::errors::SppAnnotationTargetNotACmpFunctionError>(
    //         not func_ctx, {m_scope},
    //         ERR_ARGS(*this, *m_ctx));
    //     func_ctx->builtin_annotation = true;
    // }
}


auto spp::asts::AnnotationAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Handle builtin annotations.
    const auto sym = sm->current_scope->get_var_symbol_outermost(*name).first;
    if (sym == nullptr) { return; }
    const auto fq_name = sym->fq_name()->to_string();

    // For the known builtin annotations, they will attempt to modify their contextual objects if possible, for required
    // behaviour like virtual and abstract tags on function prototypes. If the context is incorrect, nothing happens,
    // and a later stage will pickup the error, as there is a unified target-checking mechanism (custom and bulitin).
    if (fq_name == "std::annotations::compiler_builtin") {
        const auto func_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (func_ctx) { func_ctx->builtin_annotation = this; }
    }

    // Mark a visbility-enabled ast as having "public" visibility.
    else if (fq_name == "std::annotations::public") {
        const auto vis_ctx = m_ctx->to<mixins::VisibilityAst>();
        if (vis_ctx) { vis_ctx->visibility = std::make_pair(utils::Visibility::PUBLIC, this); }
    }

    // Mark a visbility-enabled ast as having "protected" visibility.
    else if (fq_name == "std::annotations::protected") {
        const auto vis_ctx = m_ctx->to<mixins::VisibilityAst>();
        if (vis_ctx) { vis_ctx->visibility = std::make_pair(utils::Visibility::PROTECTED, this); }
    }

    // Mark a visbility-enabled ast as having "private" visibility.
    else if (fq_name == "std::annotations::private") {
        const auto vis_ctx = m_ctx->to<mixins::VisibilityAst>();
        if (vis_ctx) { vis_ctx->visibility = std::make_pair(utils::Visibility::PRIVATE, this); }
    }

    // Mark a method ast as being "virtual", enabling overriding.
    else if (fq_name == "std::annotations::virtual_method") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->virtual_annotation = this; }
    }

    // Mark a method ast as being "abstract", requiring overriding.
    else if (fq_name == "std::annotations::abstract_method") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->abstract_annotation = this; }
    }

    // Mark a function ast as being "ffi", providing a linkage name.
    else if (fq_name == "std::annotations::ffi") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->ffi_annotation = this; }
    }

    // Mark a function as being inlinable via llvm.
    else if (fq_name == "std::llvm::inline") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->inline_annotation = this; }
    }

    // Mark a function as being always inlined via llvm.
    else if (fq_name == "std::llvm::always_inline") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->inline_annotation = this; }
    }

    // Mark a function as being never inlined via llvm.
    else if (fq_name == "std::llvm::noinline") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->inline_annotation = this; }
    }

    // Mark a function as being "hot" via llvm.
    else if (fq_name == "std::llvm::hot") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->temperature_annotation = this; }
    }

    // Mark a function as being "cold" via llvm.
    else if (fq_name == "std::llvm::cold") {
        const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->temperature_annotation = this; }
    }

    // TODO: "extends", "variant_default"
}


auto spp::asts::AnnotationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Convert the target into a function call to ensure it exists.
    auto fn = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(
        std::move(gn_arg_group), std::move(fn_arg_group), nullptr);
    const auto fn_ptr = fn.get();
    const auto pf = std::make_unique<PostfixExpressionAst>(ast_clone(name), std::move(fn));
    pf->stage_7_analyse_semantics(sm, meta);

    // Restore the function and generic arguments.
    gn_arg_group = std::move(fn_ptr->generic_arg_group);
    fn_arg_group = std::move(fn_ptr->arg_group);

    // Check the target function is an annotation (via "!annotation").
    const auto overload = fn_ptr->target();
    raise_if<analyse::errors::SppAnnotationTargetNotAnAnnotationError>(
        not overload->annotation_info(),
        {m_scope}, ERR_ARGS(*this, *overload));

    m_target = overload;
}


auto spp::asts::AnnotationAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load up different construct casts that an annotation may apply to.
    using analyse::utils::annotation_utils::AnnotationInfo;
    const auto annotation_info = m_target->annotation_info();
    const auto outer_mod_ctx = m_ctx->get_ast_ctx()->to<ModulePrototypeAst>();
    const auto outer_sup_ctx = m_ctx->get_ast_ctx()->to<SupPrototypeFunctionsAst>();
    const auto outer_ext_ctx = m_ctx->get_ast_ctx()->to<SupPrototypeExtensionAst>();

    // Evaluate the context that this annotation can be applied to.
    meta->save();
    const auto annotation_scope_name = INJECT_CODE("std::annotations", parse_expression);
    const auto annotation_scope = const_cast<analyse::scopes::Scope*>(
        sm->current_scope->convert_postfix_to_nested_scope(annotation_scope_name.get()));
    auto tm = ScopeManager(sm->global_scope, annotation_scope);
    annotation_info->definition->fn_arg_group->at("target")->val->stage_9_comptime_resolution(&tm, meta);
    const auto result = std::move(meta->cmp_result);
    const auto allowed_ctx = result->to<IntegerLiteralAst>()->cpp_value<std::uint64_t>();
    meta->restore();

    // Error for incompatible asts when classes are not valid targets.
    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<ClassPrototypeAst>() and not(allowed_ctx & AnnotationInfo::CLASS_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    // Error for incompatible asts when free functions are not valid targets.
    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<FunctionPrototypeAst>() and outer_mod_ctx and not(allowed_ctx & AnnotationInfo::FUNCTION_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    // Error for incompatible asts when methods are not valid targets.
    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<FunctionPrototypeAst>() and outer_sup_ctx and not(allowed_ctx & AnnotationInfo::METHOD_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    // Error for incompatible asts when overriding methods are not valid targets.
    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<FunctionPrototypeAst>() and outer_ext_ctx and not(allowed_ctx & AnnotationInfo::EXT_METHOD_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    // Error for incompatible asts when type statements are not valid targets.
    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<TypeStatementAst>() and not(allowed_ctx & AnnotationInfo::TYPE_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));

    // Error for incompatible asts when cmp statements are not valid targets.
    raise_if<analyse::errors::SppCalledAnnotationAppliedToInvalidAstError>(
        m_ctx->to<CmpStatementAst>() and not(allowed_ctx & AnnotationInfo::CMP_CTX),
        {sm->current_scope}, ERR_ARGS(*m_ctx, *this, *annotation_info->definition));
}

SPP_MOD_END
