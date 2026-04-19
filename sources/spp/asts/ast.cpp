module;
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import std;


spp::asts::Ast::Ast() = default;


spp::asts::Ast::~Ast() = default;


auto spp::asts::Ast::size() const
    -> std::size_t {
    return pos_end() - pos_start();
}


auto spp::asts::Ast::ankerl_hash() const
    -> std::size_t {
    return 0uz;
}


auto spp::asts::Ast::stage_1_pre_process(
    AbstractAst *ctx)
    -> void {
    m_ctx = static_cast<Ast*>(ctx);
}


auto spp::asts::Ast::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    m_scope = sm->current_scope;
}


auto spp::asts::Ast::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    raise<analyse::errors::SppInvalidComptimeOperationError>(
        {sm->current_scope}, ERR_ARGS(*this));
    std::unreachable();
}


auto spp::asts::Ast::stage_10_code_gen_1(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LlvmCtx *)
    -> llvm::Value* {
    return nullptr;
}


auto spp::asts::Ast::stage_11_code_gen_2(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LlvmCtx *)
    -> llvm::Value* {
    return nullptr;
}


auto spp::asts::ast_name(Ast *ast) -> std::shared_ptr<TypeAst> {
    if (const auto cls = ast->to<ClassPrototypeAst>(); cls != nullptr) {
        return cls->name;
    }
    if (const auto sup = ast->to<SupPrototypeFunctionsAst>(); sup != nullptr) {
        return sup->name;
    }
    if (const auto ext = ast->to<SupPrototypeExtensionAst>(); ext != nullptr) {
        return ext->name;
    }

    throw std::runtime_error("ast_name: Unsupported AST type " + std::string(typeid(*ast).name()));
}


auto spp::asts::ast_body(Ast *ast) -> std::vector<Ast*> {
    if (const auto cls = ast->to<ClassPrototypeAst>(); cls != nullptr) {
        return cls->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (const auto sup = ast->to<SupPrototypeFunctionsAst>(); sup != nullptr) {
        return sup->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (const auto ext = ast->to<SupPrototypeExtensionAst>(); ext != nullptr) {
        return ext->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (const auto fun = ast->to<FunctionPrototypeAst>(); fun != nullptr) {
        return fun->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }
    if (const auto mod = ast->to<ModulePrototypeAst>(); mod != nullptr) {
        return mod->impl->members
            | genex::views::ptr
            | genex::views::cast_dynamic<Ast*>()
            | genex::to<std::vector>();
    }

    // Special case for the top level scope for generic types (sup scopes are constraints).
    if (ast == nullptr) {
        return {};
    }

    throw std::runtime_error("ast_body: Unsupported AST type");
}
