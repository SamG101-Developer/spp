module spp.codegen.llvm_mangle;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.asts.cmp_statement_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_ast;
import genex;

namespace spp::codegen::mangle {
  namespace {
    auto MangleTypeNameResolvingSelf(
      spp::analyse::scopes::TypeSymbol const &type_sym)
      -> spp::Str {
      const auto name = spp::codegen::mangle::mangle_type_name(type_sym);
      if (name != "Self" or type_sym.LinkedScope == nullptr or type_sym.LinkedScope->TySym == nullptr) {
        return name;
      }

      // Guard against a "Self" that resolves to itself, which would otherwise recurse forever.
      const auto resolved = spp::codegen::mangle::mangle_type_name(*type_sym.LinkedScope->TySym);
      return resolved == "Self" ? name : resolved;
    }
  }
}

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
    | genex::views::reverse
    | genex::views::filter([](auto *scope) { return not scope->NameAsString().contains("<"); })
    | genex::views::transform([](auto *scope) { return scope->NameAsString(); })
    | genex::to<Vec>()
    | genex::views::join_with('#')
    | genex::to<Str>();
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
  // Todo: Change to use the llvm type (u32/s32 are same in llvm but different in spp).
  const auto mod_name = mangle_mod_name(owner_scope);

  // Get the return and parameter types of the function.
  const auto return_type_sym = owner_scope.GetTypeSymbol(fun_proto.ReturnType.get());
  // The variadic parameter is mangled from the tuple it actually receives, not from the single element it declares -
  // otherwise two instantiations differing only in argument count mangle to one name, and llvm quietly uniques the
  // second, leaving callers pointing at whichever one won.
  const auto variadic_param = fun_proto.FnParamGroup->GetVariadicParams();
  const auto param_type_syms = fun_proto.FnParamGroup->Params
    | genex::views::transform([&](auto const &param) {
      auto const &param_type = (fun_proto.VariadicPackType != nullptr and param.get() == static_cast<asts::FunctionParameterAst const*>(variadic_param))
        ? fun_proto.VariadicPackType
        : param->Type;
      return owner_scope.GetTypeSymbol(param_type.get());
    })
    | genex::to<Vec>();

  // Save the type symbols into a vector.
  auto types = Vec{return_type_sym};
  types.AppendRange(param_type_syms);

  // Convert the mangled type names into a single function name.
  const auto fun_sig = types
    | genex::views::transform([&](auto const &type_sym) {
      return MangleTypeNameResolvingSelf(*type_sym);
    })
    | genex::to<Vec>()
    | genex::views::join_with('#')
    | genex::to<Str>();

  // Fix for static methods as they were missing separation for
  // mangling (was merging multiple generic implementations).
  const auto owner_generic_args = owner_scope.GetGenerics();
  const auto owner_generics = owner_generic_args
    | genex::views::transform([](auto const &arg) { return arg->ToString(); })
    | genex::to<Vec>()
    | genex::views::join_with(',')
    | genex::to<Str>();
  const auto owner_name = owner_generics.empty() ? Str() : "#[" + owner_generics + "]";

  // Append the module name and function name.
  return mod_name + "#" + fun_proto.Name->Val + owner_name + "#" + fun_sig;
}
