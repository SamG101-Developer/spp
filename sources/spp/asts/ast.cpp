#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/sup_prototype_functions_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>

#include <genex/to_container.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/ptr.hpp>


auto spp::asts::ast_name(Ast *ast) -> std::shared_ptr<TypeAst> {
    if (ast_cast<ClassPrototypeAst>(ast) != nullptr) {
        return ast_cast<ClassPrototypeAst>(ast)->name;
    }
    if (ast_cast<SupPrototypeFunctionsAst>(ast) != nullptr) {
        return ast_cast<SupPrototypeFunctionsAst>(ast)->name;
    }
    if (ast_cast<SupPrototypeExtensionAst>(ast) != nullptr) {
        return ast_cast<SupPrototypeExtensionAst>(ast)->name;
    }

    throw std::runtime_error("ast_name: Unsupported AST type " + std::string(typeid(*ast).name()));
}


auto spp::asts::ast_body(Ast *ast) -> std::vector<Ast*> {
    if (ast_cast<ClassPrototypeAst>(ast) != nullptr) {
        return ast_cast<ClassPrototypeAst>(ast)->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (ast_cast<SupPrototypeFunctionsAst>(ast) != nullptr) {
        return ast_cast<SupPrototypeFunctionsAst>(ast)->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (ast_cast<SupPrototypeExtensionAst>(ast) != nullptr) {
        return ast_cast<SupPrototypeExtensionAst>(ast)->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (ast_cast<FunctionPrototypeAst>(ast) != nullptr) {
        return ast_cast<FunctionPrototypeAst>(ast)->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (ast_cast<ModulePrototypeAst>(ast) != nullptr) {
        return ast_cast<ModulePrototypeAst>(ast)->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }

    throw std::runtime_error("ast_body: Unsupported AST type");
}


spp::asts::Ast::~Ast() = default;


auto spp::asts::Ast::size() const -> std::size_t {
    return pos_end() - pos_start();
}


auto spp::asts::Ast::stage_1_pre_process(
    Ast *ctx)
    -> void {
    m_ctx = ctx;
}


auto spp::asts::Ast::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    m_scope = sm->current_scope;
}


spp::asts::Ast::Ast() = default;
