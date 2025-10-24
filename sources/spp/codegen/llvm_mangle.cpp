#include <genex/views/reverse.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/codegen/llvm_mangle.hpp>

#include <genex/to_container.hpp>
#include <genex/views/join_with.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/reverse.hpp>
#include <genex/views/transform.hpp>


auto spp::codegen::mangle::mangle_type_name(
    analyse::scopes::TypeSymbol const &type_sym)
    -> std::string {
    // Get the fully qualified name of the type symbol.
    auto fq_name = type_sym.fq_name();
    return static_cast<std::string>(*std::move(fq_name));
}


auto spp::codegen::mangle::mangle_mod_name(
    analyse::scopes::Scope const &mod_scope)
    -> std::string {
    // Generate the module name by joining the ancestor scope names with '#'.
    return mod_scope.ancestors()
        | genex::views::reverse
        | genex::views::transform([](auto *scope) { return std::get<analyse::scopes::ScopeBlockName>(scope->name).name; })
        | genex::views::materialize
        | genex::views::join_with('#')
        | genex::to<std::string>();
}


auto spp::codegen::mangle::mangle_cmp_name(
    analyse::scopes::Scope const &owner_scope,
    asts::CmpStatementAst const &cmp_stmt)
    -> std::string {
    // Qualify the "cmp" name and return the whole thing.
    const auto mod_name = mangle_mod_name(owner_scope);
    const auto cmp_name = cmp_stmt.name->val;
    return mod_name + "#" + cmp_name;
}


auto spp::codegen::mangle::mangle_fun_name(
    analyse::scopes::Scope const &owner_scope,
    asts::FunctionPrototypeAst const &fun_proto)
    -> std::string {
    // The module/context name that the function belongs to.
    const auto mod_name = mangle_mod_name(owner_scope);

    // Get the return and parameter types of the function.
    const auto return_type_sym = owner_scope.get_type_symbol(fun_proto.return_type);
    const auto param_type_syms = fun_proto.param_group->params
        | genex::views::transform([&owner_scope](auto const &param) { return owner_scope.get_type_symbol(param->type); })
        | genex::to<std::vector>();

    // Save the type symbols into a vector.
    auto types = std::vector{return_type_sym};
    types.append_range(param_type_syms);

    // Convert the mangled type names into a single function name.
    const auto fun_name = types
        | genex::views::transform([](auto const &type_sym) { return mangle_type_name(*type_sym); })
        | genex::to<std::vector>()
        | genex::views::join_with('#')
        | genex::to<std::string>();

    // Append the module name and function name.
    return mod_name + "#" + fun_name;
}
