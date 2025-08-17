#include <memory>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/mixins/visbility_enabled_ast.hpp>

#include <spp/analyse/scopes/scope_manager.hpp>


spp::asts::AnnotationAst::AnnotationAst(
    decltype(tok_at_sign) &&tok_at_sign,
    decltype(name) &&name) :
    tok_at_sign(std::move(tok_at_sign)),
    name(std::move(name)) {
}


auto spp::asts::AnnotationAst::operator==(AnnotationAst const &that) const -> bool {
    return *name == *that.name;
}


auto spp::asts::AnnotationAst::pos_start() const -> std::size_t {
    return tok_at_sign->pos_start();
}


auto spp::asts::AnnotationAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


auto spp::asts::AnnotationAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<AnnotationAst>(
        ast_clone(tok_at_sign),
        ast_clone(name));
}


spp::asts::AnnotationAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_at_sign);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::AnnotationAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_at_sign);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}


auto spp::asts::AnnotationAst::stage_1_pre_process(Ast *ctx) -> void {
    Ast::stage_1_pre_process(ctx);
    using namespace asts::utils;

    // Define some cast contexts for attribute assignment.
    const auto fun_ctx = ast_cast<FunctionPrototypeAst>(ctx);
    const auto vis_ctx = ast_cast<mixins::VisibilityEnabledAst>(ctx);

    // Mark a method as virtual.
    if (fun_ctx != nullptr and name->val == "virtual_method") {
        fun_ctx->m_virtual_annotation = this;
    }

    // Mark a method as abstract.
    else if (fun_ctx != nullptr and name->val == "abstract_method") {
        fun_ctx->m_abstract_annotation = this;
    }

    // Mark a method as non-implemented.
    else if (fun_ctx != nullptr and name->val == "no_impl") {
        fun_ctx->m_no_impl_annotation = this;
    }

    // Mark a method as non-implemented (ffi).
    else if (fun_ctx != nullptr and name->val == "ffi") {
        fun_ctx->m_no_impl_annotation = this;
    }

    // Mark a method as non-implemented (intrinsic).
    else if (fun_ctx != nullptr and name->val == "compiler_builtin") {
        fun_ctx->m_no_impl_annotation = this;
    }

    // Mark an AST as public.
    else if (vis_ctx != nullptr and name->val == "public") {
        vis_ctx->m_visibility = std::make_pair(Visibility::PUBLIC, this);
    }

    // Mark an AST as protected.
    else if (vis_ctx != nullptr and name->val == "protected") {
        vis_ctx->m_visibility = std::make_pair(Visibility::PROTECTED, this);
    }

    // Mark an AST as private.
    else if (vis_ctx != nullptr and name->val == "private") {
        vis_ctx->m_visibility = std::make_pair(Visibility::PRIVATE, this);
    }

    // Mark a function as hot.
    else if (fun_ctx != nullptr and name->val == "hot") {
        fun_ctx->m_temperature_annotation = this;
    }

    // Mark a function as cold.
    else if (fun_ctx != nullptr and name->val == "cold") {
        fun_ctx->m_temperature_annotation = this;
    }
}


auto spp::asts::AnnotationAst::stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void {
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    using namespace asts::utils;

    // Define some cast contexts for attribute assignment.
    const auto fun_ctx = ast_cast<FunctionPrototypeAst>(m_ctx);
    const auto vis_ctx = ast_cast<mixins::VisibilityEnabledAst>(m_ctx);
    const auto ctx_mod_ctx = ast_cast<ModulePrototypeAst>(m_ctx->m_ctx);
    const auto ctx_ext_ctx = ast_cast<SupPrototypeExtensionAst>(m_ctx->m_ctx);

    if (name->val == "virtual_method") {
        // The "virtual_method" annotation can only be applied to function ASTs.
        if (not fun_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-function").scopes({m_scope}).raise();
        }

        // The function ast must be a class method, not a free function.
        if (ctx_mod_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-class-method").scopes({m_scope}).raise();
        }

        // The "virtual_method" annotation cannot be applied to an abstract method.
        if (fun_ctx and fun_ctx->m_abstract_annotation) {
            analyse::errors::SppAnnotationConflictError(*this, *fun_ctx->m_abstract_annotation, *m_ctx).scopes({m_scope}).raise();
        }

        // The "virtual_method" annotation cannot be applied to a virtual method.
        if (fun_ctx and fun_ctx->m_virtual_annotation and fun_ctx->m_virtual_annotation != this) {
            analyse::errors::SppAnnotationConflictError(*this, *fun_ctx->m_virtual_annotation, *m_ctx).scopes({m_scope}).raise();
        }
    }

    else if (name->val == "abstract_method") {
        // The "abstract_method" annotation can only be applied to function ASTs.
        if (not fun_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-function").scopes({m_scope}).raise();
        }

        // The function ast must be a class method, not a free function.
        if (ctx_mod_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-class-method").scopes({m_scope}).raise();
        }

        // The "abstract_method" annotation cannot be applied to a virtual method.
        if (fun_ctx and fun_ctx->m_virtual_annotation) {
            analyse::errors::SppAnnotationConflictError(*this, *fun_ctx->m_virtual_annotation, *m_ctx).scopes({m_scope}).raise();
        }

        // The "abstract_method" annotation cannot be applied to an abstract method.
        if (fun_ctx and fun_ctx->m_abstract_annotation and fun_ctx->m_abstract_annotation != this) {
            analyse::errors::SppAnnotationConflictError(*this, *fun_ctx->m_abstract_annotation, *m_ctx).scopes({m_scope}).raise();
        }
    }

    else if (name->val == "no_impl" or name->val == "ffi" or name->val == "compiler_builtin") {
        // The "no_impl" annotation can only be applied to function ASTs.
        if (not fun_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-function").scopes({m_scope}).raise();
        }

        // The function ast must be a class method, not a free function.
        if (fun_ctx and fun_ctx->m_no_impl_annotation and fun_ctx->m_no_impl_annotation != this) {
            analyse::errors::SppAnnotationConflictError(*this, *fun_ctx->m_no_impl_annotation, *m_ctx).scopes({m_scope}).raise();
        }
    }

    else if (name->val == "public" or name->val == "protected" or name->val == "private") {
        // The visibility annotations can only be applied to ASTs that support visibility.
        if (not vis_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-visibility-enabled").scopes({m_scope}).raise();
        }

        // Access modifiers cannot be applied to methods in sup-ext blocks (only in module or sup).
        if (ctx_ext_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx->m_ctx, "extension").scopes({m_scope}).raise();
        }

        // The visibility annotation cannot be applied to an AST that already has a visibility annotation.
        if (vis_ctx and vis_ctx->m_visibility.second and vis_ctx->m_visibility.second != this) {
            analyse::errors::SppAnnotationConflictError(*this, *vis_ctx->m_visibility.second, *m_ctx).scopes({m_scope}).raise();
        }
    }

    else if (name->val == "hot" or name->val == "cold") {
        // The "hot" and "cold" annotations can only be applied to function ASTs.
        if (not fun_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-function").scopes({m_scope}).raise();
        }

        // The "hot" and "cold" annotations cannot be applied already temperate functions.
        if (fun_ctx and fun_ctx->m_temperature_annotation) {
            analyse::errors::SppAnnotationConflictError(*this, *fun_ctx->m_temperature_annotation, *m_ctx).scopes({m_scope}).raise();
        }
    }

    else if (name->val == "always_inline" or name->val == "inline" or name->val == "no_inline") {
        // The "always_inline", "inline" and "no_inline" annotations can only be applied to function ASTs.
        if (not fun_ctx) {
            analyse::errors::SppAnnotationInvalidApplicationError(*this, *m_ctx, "non-function").scopes({m_scope}).raise();
        }

        // The "always_inline", "inline" and "no_inline" annotations cannot be applied already temperate functions.
        if (fun_ctx and fun_ctx->m_inline_annotation) {
            analyse::errors::SppAnnotationConflictError(*this, *fun_ctx->m_inline_annotation, *m_ctx).scopes({m_scope}).raise();
        }
    }

    else {
        // Unknown annotations are not allowed.
        analyse::errors::SppAnnotationInvalidError(*this).scopes({m_scope}).raise();
    }
}
