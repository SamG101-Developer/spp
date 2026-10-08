module spp.codegen.llvm_mangle;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
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
import spp.utils.interner;
import genex;

namespace spp::codegen::mangle {
  namespace {
    /// A type argument by its identity: its convention, then the symbol filed under it unless that is the type being
    /// named ("naming"), else the type the identity builds as spelled. One spelling names different types from
    /// different modules, and different spellings (an alias, its target) name one type.
    auto MangleTypeId(
      analyse::scopes::Scope const &scope,
      const analyse::scopes::TypeId id,
      analyse::scopes::TypeSymbol const *naming)
      -> Str {
      const auto named = scope.TypeAstOf(id);
      const auto out = named != nullptr and named->GetConvention() != nullptr
        ? named->GetConvention()->ToString()
        : Str();
      auto const *const sym = scope.FindTypeSymbolById(analyse::scopes::BareOf(id));
      return out + (sym != nullptr and sym != naming
        ? MangleTypeName(*sym)
        : named != nullptr
        ? named->WithoutConvention()->ToString()
        : Str("?"));
    }

    /// "MangleTypeId" for a written type argument, read in "scope"; as spelled where there is no scope to read it in,
    /// or it has no identity there.
    auto MangleTypeArg(
      asts::TypeAst const &type,
      analyse::scopes::Scope const *scope,
      analyse::scopes::TypeSymbol const *naming)
      -> Str {
      const auto id = scope != nullptr ? scope->TypeIdOf(type) : nullptr;
      return id != nullptr ? MangleTypeId(*scope, id, naming) : type.ToString();
    }

    /// A comp identity ("CompKey") written out, for a value that builds no ast to print (an opaque one): a value as its
    /// literal ("V2_uz"), a comp parameter by its "ParamId" ("C12"), a pack ("P(V1_uz, C12)"), an operation over two
    /// of them ("(C12 + V1_uz)"), a constant named through a type ("M<type>.name", the type by its "TypeId"'s word), or
    /// anything else as its spelling, length-prefixed ("O5:x.y()"). The prefixes keep a value and a parameter of the
    /// same number apart in the symbol name, and the brackets keep "(a + b) * c" and "a + (b * c)" apart.
    auto MangleCompKey(
      analyse::scopes::CompKey const &node)
      -> Str {
      using Kind = analyse::scopes::CompKey::Part;
      switch (node.Kind) {
        case Kind::Value:
          if (auto const *const value = node.AsBool(); value != nullptr) { return *value ? "Vtrue" : "Vfalse"; }
          if (auto const *const value = node.AsFloat(); value != nullptr) {
            return "V" + value->ToString() + "_" + node.Text;
          }
          return "V" + node.AsInt()->ToString() + "_" + node.Text;
        case Kind::Param:
          return "C" + std::to_string(node.ParamId);
        case Kind::Opaque:
          return "O" + std::to_string(node.Text.size()) + ":" + node.Text;
        case Kind::Member:
          return "M" + std::to_string(node.Type) + "." + node.Text;
        case Kind::Pack: {
          auto out = Str("P(");
          for (auto i = 0uz; i < node.Kids.size(); ++i) {
            if (i != 0) { out += ", "; }
            out += MangleCompKey(*node.Kids[i]);
          }
          return out + ")";
        }
        case Kind::Op:
          return "(" + MangleCompKey(*node.Kids[0]) + " " + node.Text + " " + MangleCompKey(*node.Kids[1]) + ")";
        default:
          return {};
      }
    }

    /// "MangleTypeId" for a comp argument ("Scope::CompAstOf"):
    /// "1_uz + 1_uz" and "2_uz" are one value, and print alike.
    /// The identity written out where it builds no value (an
    /// opaque one).
    auto MangleCompId(
      analyse::scopes::Scope const &scope,
      const analyse::scopes::CompId id)
      -> Str {
      const auto value = scope.CompAstOf(id);
      if (value != nullptr) { return value->ToString(); }
      return id != nullptr ? MangleCompKey(*id) : Str();
    }

    /// "MangleTypeArg" for a written comp argument.
    auto MangleCompArg(
      asts::ExpressionAst const &value,
      analyse::scopes::Scope const *scope)
      -> Str {
      return scope != nullptr ? MangleCompId(*scope, scope->CompIdOf(value)) : value.ToString();
    }

    /// A generic argument list, each argument printed by its identity ("MangleTypeArg", "MangleCompArg").
    auto MangleGnArgs(
      Vec<asts::GenericArgumentAst*> const &args,
      analyse::scopes::Scope const *scope,
      analyse::scopes::TypeSymbol const *naming)
      -> Str {
      auto out = Str("[");
      for (auto i = 0uz; i < args.Len(); ++i) {
        auto const *arg = args[i];
        if (i != 0) { out += ", "; }
        if (arg->KeywordName() != nullptr) { out += arg->KeywordName()->ToString() + "="; }
        out += arg->IsTypeArg() ? MangleTypeArg(*arg->TypeVal, scope, naming) : MangleCompArg(*arg->CompVal, scope);
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
        return arg->IsTypeArg() ? ShortTypeName(*arg->TypeVal) : arg->CompVal->ToString();
      };
      const auto join = [&](auto const &list, Str const &sep) {
        auto joined = Str();
        for (auto i = 0uz; i < list.Len(); ++i) { joined += (i != 0 ? sep : Str()) + short_arg(list[i]); }
        return joined;
      };

      const auto head = bare->WithoutGns()->ToString();
      if (head == "std::tuple::Tup") { return out + "(" + join(args, ", ") + ")"; }
      if (head == "std::variant::Var" and args.Len() == 1 and args[0]->IsTypeArg()) {
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
      auto hash = static_cast<std::uint64_t>(14695981039346656037ull);
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
      // "Self" is mangled as the type it stands for: the class its linked scope belongs to, unless that is itself.
      auto const *const linked = type_sym.LinkedSymbol();
      if (not type_sym.IsSelf() or linked == &type_sym or linked->IsSelf()) { return MangleTypeName(type_sym); }
      return MangleTypeName(*linked);
    }
  }
}

auto spp::codegen::mangle::MangleTypeName(
  analyse::scopes::TypeSymbol const &type_sym)
  -> Str {
  // An instantiation is printed off its identity: its template's qualified name, then each argument as the symbol filed
  // under it (else its identity's name). Its spelled arguments, read in its own scope, would be read through its own
  // bindings, which can name the parameters they bind ("Args" bound to "Tup[FunMov[Args, Out], Args]") and grow.
  using analyse::scopes::TypeKey;
  if (type_sym.Id != nullptr and type_sym.LinkedScope != nullptr
    and analyse::scopes::HeadOf(type_sym.Id).Kind == TypeKey::Tag::Inst) {
    auto const &scope = *type_sym.LinkedScope;
    auto const *const tmpl = analyse::scopes::HeadOf(type_sym.Id).Symbol();
    auto out = tmpl->FqName()->WithoutGns()->ToString() + "[";
    auto first = true;
    for (auto const &arg : analyse::scopes::ArgsOf(analyse::scopes::HeadOf(type_sym.Id).Args)) {
      if (not first) { out += ", "; }
      first = false;
      if (arg.Named) { out += analyse::scopes::ArgNameOf(arg) + "="; }
      out += arg.TypeVal != nullptr ? MangleTypeId(scope, arg.TypeVal, &type_sym) : MangleCompId(scope, arg.CompVal);
    }
    return out + "]";
  }

  // The qualified head is built from the scope tree, so
  // it reads the same however the type was written; the
  // arguments are printed by what they resolve to
  // ("MangleGnArgs").
  const auto fq_name = type_sym.FqName();
  auto const *last = fq_name->LastTypePart();
  if (last == nullptr or last->GnArgGroup == nullptr or last->GnArgGroup->Args.IsEmpty()) { return fq_name->ToString(); }
  return fq_name->WithoutGns()->ToString()
    + MangleGnArgs(last->GnArgGroup->GetAllArgs(), type_sym.LinkedScope, &type_sym);
}

auto spp::codegen::mangle::MangleModName(
  analyse::scopes::Scope const &mod_scope)
  -> Str {
  // Generate the module name by joining the ancestor
  // scope names with '#'.
  return mod_scope.GetAncestors()
    | genex::views::reverse
    | genex::views::filter([](auto *scope) { return not scope->NameAsString().contains("<"); })
    | genex::views::transform([](auto *scope) { return scope->NameAsString(); })
    | genex::to<Vec>()
    | genex::views::join_with('#')
    | genex::to<Str>();
}

auto spp::codegen::mangle::MangleCmpName(
  analyse::scopes::Scope const &owner_scope,
  asts::CmpStatementAst const &cmp_stmt)
  -> Str {
  // Qualify the "cmp" name and return the whole thing.
  const auto mod_name = MangleModName(owner_scope);
  const auto cmp_name = cmp_stmt.Name->Val;
  return mod_name + "#" + cmp_name;
}

auto spp::codegen::mangle::MangleFnName(
  analyse::scopes::Scope const &owner_scope,
  asts::FunctionPrototypeAst const &fun_proto)
  -> Str {
  // The module/context name that the function belongs to.
  // Todo: Change to use the llvm type (u32/s32 are same in llvm but different in spp).
  const auto mod_name = MangleModName(owner_scope);

  // Get the return and parameter types of the function.
  const auto return_type_sym = owner_scope.FindTypeSymbol(fun_proto.ReturnType.get());

  // The variadic parameter is mangled from the tuple it
  // actually receives, not from the single element it
  // declares - otherwise two instantiations differing only
  // in argument count mangle to one name, and llvm quietly
  // uniques the second, leaving callers pointing at
  // whichever one won.
  const auto variadic_param = fun_proto.FnParamGroup->GetVariadicParam();
  const auto param_type_syms = fun_proto.FnParamGroup->Params
    | genex::views::transform([&](auto const &param) {
      auto const &param_type = (fun_proto.VariadicPackType != nullptr
          and param.get() == static_cast<asts::FunctionParameterAst const*>(variadic_param))
        ? fun_proto.VariadicPackType
        : param->Type;
      return owner_scope.FindTypeSymbol(param_type.get());
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
  const auto owner_generic_args = owner_scope.GetGns();
  auto owner_args = Vec<asts::GenericArgumentAst*>();
  for (auto const &arg : owner_generic_args) { owner_args.EmplaceBack(arg.get()); }
  const auto owner_name = owner_args.IsEmpty() ? Str() : "#" + MangleGnArgs(owner_args, &owner_scope, nullptr);
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
      if (arg->KeywordName() != nullptr) { readable += arg->KeywordName()->ToString() + "="; }
      if (arg->IsCompArg()) {
        readable += MangleCompArg(*arg->CompVal, &owner_scope);
        continue;
      }
      auto const *const sym = owner_scope.FindTypeSymbol(arg->TypeVal.get());
      readable += ShortTypeName(sym != nullptr ? *sym->FqName() : *arg->TypeVal);
    }
    readable += "]";
  }
  return readable + "$h" + HashHex(full);
}

auto spp::codegen::mangle::MangleClosureName(
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
