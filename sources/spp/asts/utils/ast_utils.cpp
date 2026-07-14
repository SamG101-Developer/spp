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
import spp.asts.type_ast;
import spp.utils.algorithms;

auto spp::asts::AstName(
    Ast *ast)
    -> TypeAst* {
    if (const auto cls = ast->To<ClassPrototypeAst>(); cls != nullptr) {
        return cls->Name.Get();
    }
    if (const auto sup = ast->To<SupPrototypeFunctionsAst>(); sup != nullptr) {
        return sup->Name.Get();
    }
    if (const auto ext = ast->To<SupPrototypeExtensionAst>(); ext != nullptr) {
        return ext->Name.Get();
    }

    throw std::runtime_error("ast_name: Unsupported AST type " + Str(typeid(*ast).name()));
}

auto spp::asts::AstBody(
    Ast *ast)
    -> Vec<Ast*> {
    if (const auto cls = ast->To<ClassPrototypeAst>(); cls != nullptr) {
        return cls->Impl->Members | spp::views::ptr | spp::views::cast_dynamic<Ast*> | std::ranges::to<Vec>();
    }
    if (const auto sup = ast->To<SupPrototypeFunctionsAst>(); sup != nullptr) {
        return sup->Impl->Members | spp::views::ptr | spp::views::cast_dynamic<Ast*> | std::ranges::to<Vec>();
    }
    if (const auto ext = ast->To<SupPrototypeExtensionAst>(); ext != nullptr) {
        return ext->Impl->Members | spp::views::ptr | spp::views::cast_dynamic<Ast*> | std::ranges::to<Vec>();
    }
    if (const auto fun = ast->To<FunctionPrototypeAst>(); fun != nullptr) {
        return fun->Impl->Members | spp::views::ptr | spp::views::cast_dynamic<Ast*> | std::ranges::to<Vec>();
    }
    if (const auto mod = ast->To<ModulePrototypeAst>(); mod != nullptr) {
        return mod->Impl->Members | spp::views::ptr | spp::views::cast_dynamic<Ast*> | std::ranges::to<Vec>();
    }

    // Special case for the top level scope for generic types (sup scopes are constraints).
    if (ast == nullptr) {
        return {};
    }

    throw std::runtime_error("ast_body: Unsupported AST type");
}
