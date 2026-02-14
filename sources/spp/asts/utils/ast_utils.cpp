module spp.asts.utils.ast_utils;
import spp.asts.ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_prototype_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.statement_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import genex;


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
