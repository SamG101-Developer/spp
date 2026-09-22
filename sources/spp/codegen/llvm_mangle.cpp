module spp.codegen.llvm_mangle;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import genex;

namespace spp::codegen::mangle {
  namespace {
    /// A generic argument list, printed by what each argument
    /// resolves to rather than as it was spelled: one spelling
    /// names different types from different modules, and
    /// different spellings name one type. An argument that
    /// resolves to nothing, or to the type being named, is
    /// printed as spelled.
    auto MangleGenericArgs(
      Vec<asts::GenericArgumentAst*> const &args,
      analyse::scopes::Scope const *scope,
      analyse::scopes::TypeSymbol const *naming)
      -> Str {
      auto out = Str("[");
      for (auto i = 0uz; i < args.Len(); ++i) {
        auto const *arg = args[i];
        if (i != 0) { out += ", "; }
        if (arg->Name != nullptr) { out += arg->Name->ToString() + "="; }
        if (arg->TypeVal == nullptr) {
          out += arg->CompVal->ToString();
          continue;
        }
        if (auto const *conv = arg->TypeVal->GetConvention(); conv != nullptr) { out += conv->ToString(); }
        auto const *sym = scope != nullptr ? scope->GetTypeSymbol(arg->TypeVal.get()) : nullptr;
        out += sym != nullptr and sym != naming
          ? mangle_type_name(*sym)
          : arg->TypeVal->WithoutConvention()->ToString();
      }
      return out + "]";
    }

    /// A type as a backtrace shows it: each name without its
    /// namespace, a tuple as "(A, B)", and a variant as "A or B".
    /// Only for reading - uniqueness comes from the hash on the
    /// symbol, not from this.
    auto ShortTypeName(
      asts::TypeAst const &type)
      -> Str {
      auto out = Str();
      if (auto const *conv = type.GetConvention(); conv != nullptr) { out += conv->ToString(); }
      const auto bare = type.WithoutConvention();
      auto const *last = bare->LastTypePart();
      if (last == nullptr) { return out + bare->ToString(); }

      const auto args = last->GnArgGroup != nullptr
        ? last->GnArgGroup->GetAllArgs()
        : spp::Vec<asts::GenericArgumentAst*>();
      const auto short_arg = [](asts::GenericArgumentAst const *arg) -> Str {
        return arg->TypeVal != nullptr ? ShortTypeName(*arg->TypeVal) : arg->CompVal->ToString();
      };
      const auto join = [&](auto const &list, Str const &sep) {
        auto joined = Str();
        for (auto i = 0uz; i < list.Len(); ++i) { joined += (i != 0 ? sep : Str()) + short_arg(list[i]); }
        return joined;
      };

      const auto head = bare->WithoutGenerics()->ToString();
      if (head == "std::tuple::Tup") { return out + "(" + join(args, ", ") + ")"; }
      if (head == "std::variant::Var" and args.Len() == 1 and args[0]->TypeVal != nullptr) {
        auto const *tup = args[0]->TypeVal->LastTypePart();
        if (tup != nullptr and tup->GnArgGroup != nullptr) { return out + join(tup->GnArgGroup->GetAllArgs(), " or "); }
      }

      // A closure's type is named after an address, which
      // reads as noise and changes every build.
      if (last->Name.starts_with("$closure")) { return out + "{closure}"; }
      out += last->Name;
      return args.IsEmpty() ? out : out + "[" + join(args, ", ") + "]";
    }

    /// A 64-bit FNV-1a hash as 16 hex digits: stable across
    /// builds and platforms, unlike "std::hash".
    auto HashHex(
      Str const &text)
      -> Str {
      auto hash = std::uint64_t{14695981039346656037ull};
      for (const auto c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
      }
      auto out = Str(16, '0');
      for (auto i = 16uz; i > 0; --i) {
        out[i - 1] = "0123456789abcdef"[hash & 0xf];
        hash >>= 4;
      }
      return out;
    }

    auto MangleTypeNameResolvingSelf(
      analyse::scopes::TypeSymbol const &type_sym)
      -> Str {
      const auto name = mangle_type_name(type_sym);
      if (name != "Self" or type_sym.LinkedScope == nullptr or type_sym.LinkedScope->TySym == nullptr) {
        return name;
      }

      // Guard against a "Self" that resolves to itself,
      // which would otherwise recurse forever.
      const auto resolved = mangle_type_name(*type_sym.LinkedScope->TySym);
      return resolved == "Self" ? name : resolved;
    }
  }
}

auto spp::codegen::mangle::mangle_type_name(
  analyse::scopes::TypeSymbol const &type_sym)
  -> Str {
  // The qualified head is built from the scope tree, so
  // it reads the same however the type was written; the
  // arguments are printed by what they resolve to
  // ("MangleGenericArgs").
  const auto fq_name = type_sym.FqName();
  auto text = fq_name->ToString();
  auto const *last = fq_name->LastTypePart();
  if (last == nullptr or last->GnArgGroup == nullptr or last->GnArgGroup->Args.IsEmpty()) { return text; }
  const auto spelled_args = last->GnArgGroup->ToString();
  if (not text.ends_with(spelled_args)) { return text; }
  text.resize(text.size() - spelled_args.size());
  return text + MangleGenericArgs(last->GnArgGroup->GetAllArgs(), type_sym.LinkedScope, &type_sym);
}

auto spp::codegen::mangle::mangle_mod_name(
  analyse::scopes::Scope const &mod_scope)
  -> Str {
  // Generate the module name by joining the ancestor
  // scope names with '#'.
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

  // The variadic parameter is mangled from the tuple it
  // actually receives, not from the single element it
  // declares - otherwise two instantiations differing only
  // in argument count mangle to one name, and llvm quietly
  // uniques the second, leaving callers pointing at
  // whichever one won.
  const auto variadic_param = fun_proto.FnParamGroup->GetVariadicParams();
  const auto param_type_syms = fun_proto.FnParamGroup->Params
    | genex::views::transform([&](auto const &param) {
      auto const &param_type = (fun_proto.VariadicPackType != nullptr
          and param.get() == static_cast<asts::FunctionParameterAst const*>(variadic_param))
        ? fun_proto.VariadicPackType
        : param->Type;
      return owner_scope.GetTypeSymbol(param_type.get());
    })
    | genex::to<Vec>();

  // Save the type symbols into a vector.
  auto types = Vec{return_type_sym};
  types.AppendRange(param_type_syms);

  // Convert the mangled type names into a single function
  // name.
  const auto fun_sig = types
    | genex::views::transform([&](auto const &type_sym) {
      return MangleTypeNameResolvingSelf(*type_sym);
    })
    | genex::to<Vec>()
    | genex::views::join_with('#')
    | genex::to<Str>();

  // Fix for static methods as they were missing separation
  // for mangling (was merging multiple generic implementations).
  // The owner's generics are printed by identity, as a type's
  // arguments are.
  const auto owner_generic_args = owner_scope.GetGenerics();
  auto owner_args = Vec<asts::GenericArgumentAst*>();
  for (auto const &arg : owner_generic_args) { owner_args.EmplaceBack(arg.get()); }
  const auto owner_name = owner_args.IsEmpty() ? Str() : "#" + MangleGenericArgs(owner_args, &owner_scope, nullptr);
  const auto full = mod_name + "#" + fun_proto.Name->Val + owner_name + "#" + fun_sig;

  // The readable part: the path, the name, and the owner's generics as a backtrace shows them.
  auto readable = mod_name;
  for (auto pos = readable.find('#'); pos != Str::npos; pos = readable.find('#', pos + 2)) {
    readable.replace(pos, 1, "::");
  }
  readable += "::" + fun_proto.Name->Val;
  if (not owner_args.IsEmpty()) {
    readable += "[";
    for (auto i = 0uz; i < owner_args.Len(); ++i) {
      auto const *arg = owner_args[i];
      if (i != 0) { readable += ", "; }
      if (arg->Name != nullptr) { readable += arg->Name->ToString() + "="; }
      auto const *sym = arg->TypeVal != nullptr ? owner_scope.GetTypeSymbol(arg->TypeVal.get()) : nullptr;
      readable += sym != nullptr
        ? ShortTypeName(*sym->FqName())
        : arg->TypeVal != nullptr
        ? ShortTypeName(*arg->TypeVal)
        : arg->CompVal->ToString();
    }
    readable += "]";
  }
  return readable + "$h" + HashHex(full);
}

auto spp::codegen::mangle::mangle_closure_name(
  Str const &enclosing,
  const std::size_t index)
  -> Str {
  // Nested inside the enclosing function's readable name,
  // keeping its hash, so closures of different overloads
  // stay apart and a closure within a closure reads as a
  // path.
  const auto hash_at = enclosing.rfind("$h");
  const auto readable = hash_at == Str::npos ? enclosing : enclosing.substr(0, hash_at);
  const auto hash = hash_at == Str::npos ? Str() : enclosing.substr(hash_at);
  return readable + "::{closure#" + std::to_string(index) + "}" + hash;
}
