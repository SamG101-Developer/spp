module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.annotation_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;


spp::asts::AnnotationAst::AnnotationAst(
    decltype(tok_at_sign) &&tok_at_sign,
    decltype(name) &&name) :
    tok_at_sign(std::move(tok_at_sign)),
    name(std::move(name)) {
}


auto spp::asts::AnnotationAst::pos_start() const
    -> std::size_t {
    return tok_at_sign->pos_start();
}


auto spp::asts::AnnotationAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::AnnotationAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<AnnotationAst>(
        ast_clone(tok_at_sign),
        ast_clone(name));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    return ast;
}


spp::asts::AnnotationAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_at_sign);
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
    using utils::Visibility;

    // Define some cast contexts for attribute assignment.
    const auto fun_ctx = ctx->to<FunctionPrototypeAst>();
    const auto vis_ctx = ctx->to<mixins::VisibilityEnabledAst>();

    // Mark a method as virtual.
    if (fun_ctx != nullptr and name->val == "virtual_method") {
        fun_ctx->virtual_annotation = this;
    }

    // Mark a method as abstract.
    else if (fun_ctx != nullptr and name->val == "abstract_method") {
        fun_ctx->abstract_annotation = this;
    }

    // Mark a method as non-implemented (ffi).
    else if (fun_ctx != nullptr and name->val == "ffi") {
        fun_ctx->no_impl_annotation = this;
    }

    // Mark a method as non-implemented (intrinsic).
    else if (fun_ctx != nullptr and name->val == "compiler_builtin") {
        fun_ctx->no_impl_annotation = this;
    }

    // Mark an AST as public.
    else if (vis_ctx != nullptr and name->val == "public") {
        vis_ctx->visibility = std::make_pair(Visibility::PUBLIC, this);
    }

    // Mark an AST as protected.
    else if (vis_ctx != nullptr and name->val == "protected") {
        vis_ctx->visibility = std::make_pair(Visibility::PROTECTED, this);
    }

    // Mark an AST as private.
    else if (vis_ctx != nullptr and name->val == "private") {
        vis_ctx->visibility = std::make_pair(Visibility::PRIVATE, this);
    }

    // Mark a function as hot.
    else if (fun_ctx != nullptr and name->val == "hot") {
        fun_ctx->temperature_annotation = this;
    }

    // Mark a function as cold.
    else if (fun_ctx != nullptr and name->val == "cold") {
        fun_ctx->temperature_annotation = this;
    }
}


auto spp::asts::AnnotationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Default AST processing (sets scope).
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    using utils::Visibility;

    // Define some cast contexts for attribute assignment.
    const auto fun_ctx = m_ctx->to<FunctionPrototypeAst>();
    const auto vis_ctx = m_ctx->to<mixins::VisibilityEnabledAst>();
    const auto ctx_mod_ctx = m_ctx->get_ast_ctx()->to<ModulePrototypeAst>();
    const auto ctx_ext_ctx = m_ctx->get_ast_ctx()->to<SupPrototypeExtensionAst>();

    if (name->val == "virtual_method") {
        // The "virtual_method" annotation can only be applied to function ASTs.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
            not fun_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx, "non-function"));

        // The function ast must be a class method, not a free function.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
            ctx_mod_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx, "non-class-method"));

        // The "virtual_method" annotation cannot be applied to an abstract method.
        raise_if<analyse::errors::SppAnnotationConflictError>(
            fun_ctx and fun_ctx->abstract_annotation, {m_scope},
            ERR_ARGS(*this, *fun_ctx->abstract_annotation, *m_ctx));

        // The "virtual_method" annotation cannot be applied to a virtual method.
        raise_if<analyse::errors::SppAnnotationConflictError>(
            fun_ctx and fun_ctx->virtual_annotation and fun_ctx->virtual_annotation != this, {m_scope},
            ERR_ARGS(*this, *fun_ctx->virtual_annotation, *m_ctx));
    }

    else if (name->val == "abstract_method") {
        // The "abstract_method" annotation can only be applied to function ASTs.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
            not fun_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx, "non-function"));

        // The function ast must be a class method, not a free function.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
            ctx_mod_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx, "non-class-method"));

        // The "abstract_method" annotation cannot be applied to a virtual method.
        raise_if<analyse::errors::SppAnnotationConflictError>(
             fun_ctx and fun_ctx->virtual_annotation, {m_scope},
             ERR_ARGS(*this, *fun_ctx->virtual_annotation, *m_ctx));

        // The "abstract_method" annotation cannot be applied to an abstract method.
        raise_if<analyse::errors::SppAnnotationConflictError>(
             fun_ctx and fun_ctx->abstract_annotation and fun_ctx->abstract_annotation != this, {m_scope},
             ERR_ARGS(*this, *fun_ctx->abstract_annotation, *m_ctx));
    }

    else if (name->val == "ffi" or name->val == "compiler_builtin") {
        // The "ffi/compiler_builtin" annotation can only be applied to function ASTs.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
             not fun_ctx, {m_scope},
             ERR_ARGS(*this, *m_ctx, "non-function"));
    }

    else if (name->val == "public" or name->val == "protected" or name->val == "private") {
        // The visibility annotations can only be applied to ASTs that support visibility.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
            not vis_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx, "non-visibility-enabled"));

        // Access modifiers cannot be applied to methods in sup-ext blocks (only in module or sup).
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
            ctx_ext_ctx, {m_scope},
            ERR_ARGS(*this, *m_ctx->get_ast_ctx(), "extension"));

        // The visibility annotation cannot be applied to an AST that already has a visibility annotation.
        raise_if<analyse::errors::SppAnnotationConflictError>(
            vis_ctx and vis_ctx->visibility.second and vis_ctx->visibility.second != this, {m_scope},
            ERR_ARGS(*this, *vis_ctx->visibility.second, *m_ctx));
    }

    else if (name->val == "hot" or name->val == "cold") {
        // The "hot" and "cold" annotations can only be applied to function ASTs.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
             not fun_ctx, {m_scope},
             ERR_ARGS(*this, *m_ctx, "non-function"));

        // The "hot" and "cold" annotations cannot be applied already temperate functions.
        raise_if<analyse::errors::SppAnnotationConflictError>(
             fun_ctx and fun_ctx->temperature_annotation and fun_ctx->temperature_annotation != this, {m_scope},
             ERR_ARGS(*this, *fun_ctx->temperature_annotation, *m_ctx));
    }

    else if (name->val == "always_inline" or name->val == "inline" or name->val == "no_inline") {
        // The "always_inline", "inline" and "no_inline" annotations can only be applied to function ASTs.
        raise_if<analyse::errors::SppAnnotationInvalidApplicationError>(
             not fun_ctx, {m_scope},
             ERR_ARGS(*this, *m_ctx, "non-function"));

        // The "always_inline", "inline" and "no_inline" annotations cannot be applied already temperate functions.
        raise_if<analyse::errors::SppAnnotationConflictError>(
             fun_ctx and fun_ctx->inline_annotation, {m_scope},
             ERR_ARGS(*this, *fun_ctx->inline_annotation, *m_ctx));
    }

    else {
        // Unknown annotations are not allowed.
        raise<analyse::errors::SppAnnotationInvalidError>({m_scope}, ERR_ARGS(*this));
    }
}
