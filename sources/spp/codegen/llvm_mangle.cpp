module spp.codegen.llvm_mangle;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.asts.cmp_statement_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;

auto spp::codegen::mangle::mangle_type_name(
    analyse::scopes::TypeSymbol const &type_sym)
    -> Str {
    // Get the fully qualified name of the type symbol.
    const auto fq_name = type_sym.FqName();
    return fq_name->ToString();
}

auto spp::codegen::mangle::mangle_mod_name(
    analyse::scopes::Scope const &mod_scope)
    -> Str {
    // Generate the module name by joining the ancestor scope names with '#'.
    return mod_scope.Ancestors()
        | std::views::reverse
        | std::views::filter([](auto *scope) { return not scope->NameAsString().contains("<"); })
        | std::views::transform([](auto *scope) { return scope->NameAsString(); })
        | std::views::join_with('#')
        | std::ranges::to<Str>();
}

auto spp::codegen::mangle::mangle_cmp_name(
    analyse::scopes::Scope const &owner_scope,
    asts::CmpStatementAst const &cmp_stmt)
    -> Str {
    // Qualify the "cmp" name and return the whole thing.
    const auto mod_name = mangle_mod_name(owner_scope);
    const auto cmp_name = cmp_stmt.Name->Val;
    return mod_name + "#" + cmp_name;
}

auto spp::codegen::mangle::mangle_fun_name(
    analyse::scopes::Scope const &owner_scope,
    asts::FunctionPrototypeAst const &fun_proto)
    -> Str {
    // The module/context name that the function belongs to.
    const auto mod_name = mangle_mod_name(owner_scope);

    // Get the return and parameter types of the function.
    const auto return_type_sym = owner_scope.GetTypeSymbol(fun_proto.ReturnType.Get());
    const auto param_type_syms = fun_proto.FnParamGroup->Params
        | std::views::transform([&owner_scope](auto const &param) { return owner_scope.GetTypeSymbol(param->Type.Get()); })
        | std::ranges::to<Vec>();

    // Save the type symbols into a vector.
    auto types = Vec{return_type_sym};
    types.AppendRange(param_type_syms);

    // Convert the mangled type names into a single function name.
    const auto fun_sig = types
        | std::views::transform([](auto const &type_sym) { return mangle_type_name(*type_sym); })
        | std::views::join_with('#')
        | std::ranges::to<Str>();

    // Append the module name and function name.
    return mod_name + "#" + fun_proto.Name->Val + "#" + fun_sig;
}
