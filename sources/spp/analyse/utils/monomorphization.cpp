module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.monomorphization;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_tree;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.aliases;
import spp.analyse.utils.packs;
import spp.analyse.utils.self_type;
import spp.analyse.utils.sup_attachment;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.utils.ice;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;

namespace spp::analyse::utils::monomorphization {
  namespace {
    /// The templates waiting, in the order they were recorded.
    auto _Pending = Vec<FunctionPrototypeAst*>();

    /// The index of the next template to be drained.
    auto _Cursor = 0uz;

    /// Membership of the unconsumed part of the pending templates.
    auto _Queued = Set<FunctionPrototypeAst*>();

    /**
     * Type an instantiation's parameter symbols from its signature, once the signature is substituted. The symbols are
     * copies of the template's, which stage 6 typed (it only ever runs on the template); the signature is what the
     * instantiation is, so each takes its parameter's type from it:
     * - "Self", pinned to a call's receiver, is pinned into the signature first: "self" is the receiver (its symbol
     *   with "self"'s convention), and a parameter written against "Self" ("source: &Self" in an inherited
     *   "clone_from") means it too. Unpinned, the signature keeps "Self" as written (it is substituted per call,
     *   "PassedOverload::SelfId"), so a symbol written against it keeps the class its scope read "Self" as.
     * - A variadic parameter declares one element but binds the tuple the call collected its trailing arguments into.
     * - A parameter bound to a callable keeps the interface its generic's constraint promised ("IndirectFnType"): a
     *   "FunMut" satisfies "FunMov" while also being callable through a borrow, so reading the substituted type would
     *   make this body consume the value here and borrow it in the next instantiation, which linear ownership cannot
     *   account for. The constraint lives on the template's generic parameter, which nothing in the instantiation
     *   refers to any more.
     * A destructuring parameter binds each name to a part of its type, which its scope read already, so is left.
     * @param new_fn_proto The instantiated prototype, whose signature is substituted and pinned in place.
     * @param fn_proto The template, which still carries the generic constraints.
     * @param pinned_self What the instantiation pins "Self" to (a call's receiver), or @c nullptr .
     * @param variadic_pack_type The tuple the call collapsed its trailing arguments into, or @c nullptr .
     * @param tm A scope manager positioned at the instantiation's scope.
     * @param meta Associated metadata.
     */
    auto TypeParamSymbolsFromSignature(
      FunctionPrototypeAst &new_fn_proto, FunctionPrototypeAst const &fn_proto,
      Shared<TypeAst> const &pinned_self,
      Shared<TypeAst> const &variadic_pack_type, ScopeManager &tm,
      meta::CompilerMetaData *meta) -> void {
      auto const *const new_fn_scope = tm.CurrentScope;
      auto const &params_scope = *new_fn_scope->Children[0];
      auto const *const self_param = new_fn_proto.FnParamGroup->GetSelfParam();
      const auto symbol_of = [&](IdentifierAst const &name) { return params_scope.FindVarSymbol(&name, true); };

      // "Self" pinned into the signature.
      if (pinned_self != nullptr) {
        for (auto const &param : new_fn_proto.FnParamGroup->Params) {
          if (param.get() == self_param) {
            auto substituted_self = AstCloneShared(pinned_self);
            substituted_self->Stage7_AnalyseSemantics(&tm, meta);
            param->Type = substituted_self;
            if (auto *const sym = symbol_of(*param->ExtractName()); sym != nullptr) {
              sym->Type = substituted_self->WithConvention(AstClone(self_param->Conv));
            }
          }
          else if (param->Type != nullptr and type_predicates::DoesTypeNameSelf(*param->Type)) {
            param->Type = self_type::SubstituteSelf(*param->Type, pinned_self.get(), *new_fn_scope, &tm, meta);
          }
        }
      }
      new_fn_proto.VariadicPackType = AstClone(variadic_pack_type);

      // Each named parameter's symbol, from its parameter.
      auto const *const variadic_param = new_fn_proto.FnParamGroup->GetVariadicParam();
      for (auto const *p : new_fn_proto.FnParamGroup->GetNonSelfParams()) {
        if (p->Var->To<LocalVariableSingleIdentifierAst>() == nullptr) { continue; }
        auto *const sym = symbol_of(*p->ExtractName());
        if (sym == nullptr) { continue; }
        if (p == variadic_param and variadic_pack_type != nullptr) {
          auto pack_type = AstClone(variadic_pack_type);
          pack_type->Stage7_AnalyseSemantics(&tm, meta);
          sym->Type = std::move(pack_type);
        }
        else if (not type_predicates::DoesTypeNameSelf(*p->Type)) { sym->Type = p->Type; }

        // The interface a callable parameter's constraint promised, read as the parameter's own type is: the
        // constraint is written in the template's terms ("FunMov[(T,), U]"), and what the call needs is this
        // instantiation's argument and return types.
        const auto declared = p->Source.OriginalType;
        if (declared == nullptr) { continue; }
        const auto gn_param = genex::find_if(
          fn_proto.GnParamGroup->Params, [&](auto const &g) { return *g->Name == *declared; });
        if (gn_param == fn_proto.GnParamGroup->Params.end() or (*gn_param)->TypeConstraints == nullptr) { continue; }
        for (auto const &c : (*gn_param)->TypeConstraints->Constraints) {
          if (not type_predicates::IsTypeFunction(TypeRef::ForKindCheck(*c, *new_fn_scope), *new_fn_scope)) {
            continue;
          }
          auto fn_type = type_resolution::ReadType(*c, ExprSubst::In(*new_fn_scope));
          fn_type->Stage7_AnalyseSemantics(&tm, meta);

          // Through an alias, as the name of what it resolves to
          // ("TypeRef" never answers with an alias).
          const auto head = new_fn_scope->FindHeadSymbol(*fn_type);
          auto const *const full = TypeRef::Of(*fn_type, *new_fn_scope).Symbol;
          sym->IndirectFnType = head != nullptr and head->Alias != nullptr and full != nullptr
            ? AstCloneShared(full->FqName())
            : fn_type;
          break;
        }
      }
    }

    /**
     * Whether an instantiation is concrete: every argument it pins names a real type or value, and every type in its
     * own signature resolves to one. A non-concrete instantiation is still a template as far as anything downstream is
     * concerned, so codegen has nothing to emit for it.
     * @param args_id The identity of the arguments this instantiation pins.
     * @param new_fn_proto The instantiated prototype whose signature is checked.
     * @param instance_self What "Self" is in the instantiation, or @c nullptr .
     * @param tm A scope manager positioned at the instantiation's scope.
     * @param meta Associated metadata.
     * @return If the instantiation is fully concrete.
     */
    auto ComputeIsConcrete(
      const TypeId args_id,
      FunctionPrototypeAst const &new_fn_proto,
      TypeAst const *instance_self,
      ScopeManager &tm,
      meta::CompilerMetaData *meta)
      -> bool {
      auto const *const new_fn_scope = tm.CurrentScope;
      const auto type_is_concrete = [&](TypeAst const &type) {
        const auto resolved = self_type::SubstituteSelf(type, instance_self, *new_fn_scope, &tm, meta);
        return type_predicates::IsTypeConcrete(*resolved, *new_fn_scope);
      };

      // The arguments are asked the same way a class instantiation
      // asks them; a function goes on to check the signature it
      // ended up with, which a class has no equivalent of. Each is
      // read through the instance's own bindings first: an argument
      // defaulted to another ("Rhs=Self" on "Eq") names that one's
      // parameter, and is whatever the instance binds it to.
      const auto bindings = scopes::BindingsOfArgs(args_id);
      const auto args_concrete = genex::all_of(scopes::ArgsOf(args_id), [&](auto const &arg) {
        return arg.TypeVal != nullptr
          ? scopes::IsConcreteTypeId(scopes::SubstituteTypeId(arg.TypeVal, bindings))
          : scopes::IsConcreteCompId(scopes::SubstituteCompId(arg.CompVal, bindings));
      });
      return args_concrete
        and type_is_concrete(*new_fn_proto.ReturnType)
        and genex::all_of(new_fn_proto.FnParamGroup->GetAllParams(), [&](auto *p) {
          return type_is_concrete(*p->Type);
        });
    }

    /**
     * Give a scope its own copy of the symbols it was cloned from. A clone starts out sharing the template's symbol
     * objects (see @c Scope 's copy constructor), so it must be handed its own before anything substitutes a binding
     * into one, or it would rewrite the template's symbol and leave the template - and every other instantiation -
     * seeing this instantiation's types.
     * @param dst The cloned scope being given its own symbols.
     * @param src The template scope it was cloned from.
     * @param recurse Whether to do the same for the cloned subtree, or only for @p dst itself.
     */
    auto GiveScopeOwnSymbols(
      Scope &dst,
      Scope const &src,
      const bool recurse)
      -> void {
      dst.InternalTable.DeepCopyFrom(src.InternalTable);
      if (not recurse) { return; }
      for (auto i = 0uz; i < dst.Children.size() and i < src.Children.size(); ++i) {
        GiveScopeOwnSymbols(*dst.Children[i].get(), *src.Children[i].get(), true);
      }
    }

    /**
     * The first step of every instantiation: copy the template's scope, "depth" levels down (a class's alone, a
     * function's with its own scope below the mock block, a "sup" block's with each member's mock block and function
     * scope), and give the copy its own symbols - below it too when "own_below" - so substituting into them leaves the
     * template's alone ("GiveScopeOwnSymbols"). Nothing below "depth" is copied: a body is analysed afresh where it is
     * instantiated.
     * @param tmpl The template's scope.
     * @param depth How many levels of the template's descendants to copy.
     * @param own_below Whether the copied descendants get their own symbols too, or share the template's.
     * @return The copy, not yet attached to the scope tree.
     */
    auto CopyTemplateScope(
      Scope const &tmpl,
      const std::size_t depth,
      const bool own_below)
      -> Unique<Scope> {
      auto copy = MakeUnique<Scope>(tmpl, depth);
      GiveScopeOwnSymbols(*copy, tmpl, own_below);
      return copy;
    }

    /**
     * Record an instantiation's own name in terms that mean the same wherever it is read. A type argument that is
     * closed where it is written - a generic bound to a closed type, or a type naming one ("ControlBlock[T]" with "T"
     * bound to "S32") - records that type, and keeps its spelling: the name is only printed, while every lookup (and
     * expression substitution, which leaves a name recording anything but a parameter alone) follows the recorded
     * identity. One still open stays as written: an open name built from the minting scope's bindings would grow when
     * read again ("Single[Arr[T]]" inside "Single"). A comp argument whose value is closed is written as what it folds
     * to, the comp form of the same record: a comp value has no written identity to stamp, as its identity is its
     * folded spelling ("Scope::CompKey"), and its spelling is what is mangled and printed.
     * @param args The instantiation's own arguments, a copy private to its name.
     * @param scope The scope the instantiation is made from, whose bindings the arguments are read through.
     */
    auto RecordClosedBindings(
      GenericArgumentGroupAst &args,
      Scope const &scope)
      -> void {
      for (auto &arg : args.Args) {
        if (arg->KeywordName() == nullptr) { continue; }
        if (arg->IsCompArg()) {
          if (auto folded = scope.FoldedCompAstOf(*arg->CompVal); folded != nullptr) {
            arg->CompVal = std::move(folded);
          }
          continue;
        }
        if (arg->TypeVal->IsSelfType()) { continue; }

        // Only what depends on this scope's bindings: a generic,
        // or a type naming one. Anything else means the same
        // wherever it is read already, and one read here too early
        // ("U8" before its instance is made) would record the
        // template it reaches.
        auto const *const val_sym = scope.FindTypeSymbol(arg->TypeVal.get());
        const auto written = arg->TypeVal->LastTypePart()->StampedTypeId();
        const auto names_generics = written != nullptr and DoesTypeIdNameAnyGnParams(written);
        if (not names_generics and (val_sym == nullptr or not val_sym->IsGn())) { continue; }

        // Its identity, not the class a binding links: one
        // still waiting on an alias's target ("T=U8" before
        // the "SizedInteger" instance is made) links only
        // that target's template.
        const auto ref = TypeRef::Of(*arg->TypeVal, scope);
        if (ref.Id == nullptr or ref.Symbol == nullptr or ref.Symbol->IsBareTemplate()) { continue; }
        if (not type_predicates::IsTypeConcrete(ref)) { continue; }
        const auto bound = scopes::BareOf(ref.Id);
        arg->TypeVal->StampTypeId(bound);
        arg->TypeVal->LastTypePart()->StampTypeId(bound);
      }
    }

    /**
     * The name an argument of an arguments' identity ("Scope::ArgsIdOf") binds under: its parameter's own name, else
     * one spelled from its identity ("ArgNameOf": "Self", or a variadic pack's), which a parameter's records the
     * identity of, as the parameter's own name does.
     */
    auto BindingTypeName(
      scopes::TypeIdArg const &arg)
      -> Shared<TypeIdentifierAst> {
      SPP_ASSERT(arg.Named);
      const auto by_param = arg.Slot != 0;
      if (by_param) {
        if (auto const *const param = scopes::FindGnTypeParamById(arg.Slot); param != nullptr) { return param->Name; }
      }
      auto name = MakeShared<TypeIdentifierAst>(0, scopes::ArgNameOf(arg), nullptr);
      if (by_param) { name->StampTypeId(scopes::ParamTypeId(arg.Slot)); }
      return name;
    }

    /**
     * "BindingTypeName" for a comp argument.
     */
    auto BindingCompName(
      scopes::TypeIdArg const &arg)
      -> Shared<IdentifierAst> {
      SPP_ASSERT(arg.Named);
      if (arg.Slot != 0) {
        if (auto const *const param = scopes::FindGnCompParamById(arg.Slot); param != nullptr) { return param->Name; }
      }
      return MakeShared<IdentifierAst>(0, scopes::ArgNameOf(arg));
    }

    /// [CHECKED]
    /// Generate a "type id" from the generic arguments embedded
    /// into the "args_id", with some additional modifications;
    /// the self-referential params like "T=T" are dropped, and
    /// the variadic pack tuple is appended (variadic function
    /// parameters, not generics).
    auto CallArgsId(
      FunctionPrototypeAst const &fn_proto, const TypeId args_id,
      Shared<TypeAst> const &variadic_pack_type,
      Scope const *scope) -> TypeId {
      // Check if the type ids or the comp ids are a matching
      // pair (self-stating), allowing them to be filtered.
      const auto restates_own_param = [](scopes::TypeIdArg const &arg) {
        if (not arg.Named or arg.Slot == 0) { return false; }
        return arg.TypeVal != nullptr
          ? arg.TypeVal == scopes::ParamTypeId(arg.Slot)
          : arg.CompVal == scopes::ParamCompId(arg.Slot);
      };

      // Prepare the new type key to be built for the function call
      // args id. Re-add all the type and comp generic args into it,
      // unless they are self-stating.
      auto key = scopes::TypeKey();
      for (auto const &arg : scopes::ArgsOf(args_id)) {
        if (restates_own_param(arg)) { continue; }
        const auto tag = arg.Named
          ? scopes::TypeKey::Tag::Arg
          : scopes::TypeKey::Tag::Pos;

        key.Push(tag, arg.Slot);
        if (arg.TypeVal != nullptr) { key.PushTypePart(arg.TypeVal); }
        if (arg.CompVal != nullptr) { key.PushCompPart(arg.CompVal); }
      }

      // The variadic pack type is the type on a variadic function
      // parameter as a tuple. This needs to be added into the type
      // id.
      if (variadic_pack_type != nullptr) {
        const auto pack = fn_proto.FnParamGroup->GetVariadicParam();
        const auto pack_id = GnPackParamId(
          pack, packs::FnVariadicParamName(*pack->ExtractName()));
        key.Push(scopes::TypeKey::Tag::Arg, pack_id);
        key.PushTypePart(scope->PartialTypeIdOf(*variadic_pack_type));
      }

      return key.Words.empty() ? nullptr : InternTypeKey(std::move(key));
    }

    /**
     * Create the symbol that binds a type parameter to "value", the identity of the type given for it, held as that
     * identity holds it (a binding to "&Str" is borrowed).
     * @param name The parameter's name, which the binding is registered under.
     * @param value The identity of the type bound.
     * @param sm The scope manager whose current scope the value is read in.
     * @param meta The compiler meta data.
     * @return The symbol for the binding.
     */
    auto CreateGnTypeSymbol(
      Shared<TypeIdentifierAst> const &name,
      const TypeId value,
      ScopeManager &sm,
      meta::CompilerMetaData &meta)
      -> Shared<TypeSymbol> {
      // "Self" itself is left for what pins it, linking nothing; it records that it is "Self", so it keys as "Self"
      // ("KeyBinding"), what the parameter means here, rather than as a binding of the parameter.
      if (value == scopes::ParamTypeId(0)) {
        auto self_binding = MakeShared<TypeSymbol>(name, nullptr, nullptr, sm.CurrentScope, TypeKind::GnTypeArg);
        self_binding->BoundTypeVal = sm.CurrentScope->TypeAstOf(value);
        return self_binding;
      }

      // What is filed under the identity. Only where nothing usable is (an instantiation not made yet, or an alias whose
      // target is not resolved yet) is its spelling read, which makes what is missing: an alias whose own stage 4 has
      // not run ("U8" bound in "Str" before the number module's aliases are resolved) is resolved now
      // ("TypeStatementAst::ResolveTargetEarly"), so a binding is always made to the target itself.
      auto const &scope = *sm.CurrentScope;
      const auto written = scope.TypeAstOf(value);
      auto *true_val_sym = scope.FindTypeSymbolById(scopes::BareOf(value));
      if ((true_val_sym == nullptr or not true_val_sym->IsBindTarget()) and written != nullptr) {
        if (auto *const found = scope.FindTypeSymbol(written.get()); found != nullptr) { true_val_sym = found; }
        auto ref = TypeRef::Of(*written, scope);
        if ((ref.Symbol == nullptr or not ref.Symbol->IsBindTarget()) and true_val_sym != nullptr
          and true_val_sym->Alias != nullptr and true_val_sym->Alias->Stmt != nullptr) {
          true_val_sym->Alias->Stmt->ResolveTargetEarly(*true_val_sym, sm, meta);
          ref = TypeRef::Of(*written, scope);
        }
        if (ref.Symbol != nullptr and ref.Symbol->IsBindTarget()) { true_val_sym = ref.Symbol; }

        // An instance not made yet ("SizedInteger[w=8_u32, ..]" for "U8", before anything has named it) is made by
        // analysing it, as an alias's target is when it is resolved early, from a clean context; the checks are
        // relaxed as for any substituted type ("AnalyseSubstitutedType").
        if ((true_val_sym == nullptr or not true_val_sym->IsBindTarget())
          and scopes::HeadOf(value).Kind == TypeKey::Tag::Inst) {
          const auto made = AstCloneShared(written);
          {
            const auto _meta_guard = meta::MetaGuard(&meta);
            meta.ResetContext();
            meta.AllowAbstractType = true;
            meta.IgnoreAccessModifierViolations = true;
            meta.SkipSubstitutedConstraintChecks = true;
            made->Stage7_AnalyseSemantics(&sm, &meta);
          }
          ref = TypeRef::Of(*made, scope);
          if (ref.Symbol != nullptr and ref.Symbol->IsBindTarget()) { true_val_sym = ref.Symbol; }
        }
      }

      auto sym = MakeShared<TypeSymbol>(
        name, nullptr, nullptr, sm.CurrentScope, TypeKind::GnTypeArg, false, asts::utils::Visibility::kPublic,
        static_cast<asts::ConventionTag>(scopes::HeadOf(value).Conv));
      if (true_val_sym != nullptr) { sym->BindTo(*true_val_sym); }

      // Record what the parameter was bound to. When the value
      // is another (unresolved) generic parameter there is no
      // linked scope to recover the binding from later, so the
      // value type is the only record of it.
      sym->BoundTypeVal = written;
      return sym;
    }

    /**
     * "CreateGnTypeSymbol" for a comp argument: a variable symbol carrying the bound comp-time value, typed as its
     * parameter is declared ("cmp n: USize"), which the value has been checked against; a pack ("cmp ..ns: USize") is
     * the tuple of that type, one per element its value holds, as a type pack is bound to a tuple. Only a parameter
     * typed by another ("cmp n: T") is typed by its value, which may not be readable yet: a constant named through a
     * type ("Self::n") is only reachable once the "sup" blocks are attached.
     * @param name The parameter's name, which the binding is registered under.
     * @param value The identity of the value bound.
     * @param sm The scope manager whose current scope the value is read in.
     * @param meta The compiler meta data.
     * @param param The parameter's symbol, if the scope declares the parameter.
     * @return The symbol for the binding.
     */
    auto CreateGnCompSymbol(
      Shared<IdentifierAst> const &name,
      const CompId value,
      ScopeManager &sm,
      meta::CompilerMetaData *meta,
      VariableSymbol const *param)
      -> Shared<VariableSymbol> {
      // The value as its identity spells it: folded already where it folds ("n + 1_uz" with "n" bound to "1_uz" is
      // "2_uz"), and a bound comp generic it names read as what it is bound to where it was keyed ("SizedInteger[w]" in
      // a "[cmp w: U32]" instance binds 32).
      auto bound = sm.CurrentScope->CompAstOf(value);
      if (bound == nullptr) { Ice(*name, *sm.CurrentScope, "a comp argument's value has no expression to spell it"); }

      auto const *const declared = param != nullptr ? param->Type.get() : nullptr;
      const auto declared_id = declared != nullptr ? sm.CurrentScope->TypeIdOf(*declared) : nullptr;
      auto type = Shared<TypeAst>(nullptr);
      // A pack forwarded unbound ("A[ns]" in the template) is
      // not a tuple yet, and keeps its element type.
      if (not scopes::IsClosedTypeId(declared_id)) { type = bound->InferType(&sm, meta); }
      else if (not param->IsVariadic or bound->To<asts::TupleLiteralAst>() == nullptr) {
        type = AstCloneShared(declared);
      }
      else {
        auto elems = Vec<Shared<TypeAst>>();
        for (auto i = 0uz; i < packs::CompPackElements(*bound).size(); ++i) {
          elems.emplace_back(AstCloneShared(declared));
        }
        type = asts::generate::common_types::TupleType(declared->PosStart(), std::move(elems));
      }

      auto sym = MakeShared<VariableSymbol>(
        name, std::move(type), sm.CurrentScope, VariableKind::GnCompArg, false, asts::utils::Visibility::kPublic);
      sym->CompTimeValue = std::move(bound);

      // Its identity, keyed here once, as a type binding's link
      // is resolved where its argument is written: reading the
      // binding then needs no lookup and no scope. A value naming
      // a constant that cannot be read yet keys differently once
      // it can, so it is left to be keyed where it is read.
      const auto bound_id = sm.CurrentScope->CompIdOf(*sym->CompTimeValue->To<ExpressionAst>());
      sym->BoundCompId = scopes::IsReadableCompId(bound_id) ? bound_id : nullptr;
      return sym;
    }

    /**
     * Record which parameter a binding binds: the one the scope declares under its name ("old", the symbol the binding
     * replaces - the parameter itself, or, in a copy of an instantiation as one made through a "use" of a generic
     * class is, a binding of it, which already names the parameter), or else one inherited from an enclosing "sup"
     * block, which was never declared here and is reached through the parents ("inherited", only asked when needed).
     */
    auto BindParam(
      auto &binding,
      auto const *old,
      auto &&inherited)
      -> void {
      if (old != nullptr and old->ParamId() != 0) { binding.BindsParamId = old->ParamId(); }
      else if (auto const *const param = inherited(); param != nullptr) { binding.BindsParamId = param->ParamId(); }
    }

    /**
     * Register a type binding into an instantiation's scope, replacing the template's own (unbound) parameter symbol
     * of the same name. A type parameter's constraints are written on the template, so they are carried over.
     */
    auto RegisterGnTypeSymbol(
      Scope &scope,
      Shared<TypeSymbol> const &binding)
      -> void {
      const auto old = scope.RemTypeSymbol(binding->Name.get());
      if (old != nullptr) {
        binding->TypeConstraints = AstCloneVecShared(old->TypeConstraints);
        binding->IsVariadic = old->IsVariadic;
      }
      BindParam(*binding, old.get(), [&] {
        return scope.Parent != nullptr ? scope.Parent->FindTypeSymbol(binding->Name.get()) : nullptr;
      });
      scope.AddTypeSymbol(binding);
    }

    /**
     * "RegisterGnTypeSymbol" for a comp binding.
     */
    auto RegisterGnCompSymbol(
      Scope &scope,
      Shared<VariableSymbol> const &binding)
      -> void {
      const auto old = scope.RemVarSymbol(binding->Name.get());
      if (old != nullptr) { binding->IsVariadic = old->IsVariadic; }
      BindParam(*binding, old.get(), [&] {
        return scope.Parent != nullptr ? scope.Parent->FindVarSymbol(binding->Name.get()) : nullptr;
      });
      scope.AddVarSymbol(binding);
    }

    /// [CHECKED]
    /// The shared second step of all instantiation: bind the
    /// generic parameters, against the arguments lists one after
    /// the other: for each generic argument, create a generic
    /// symbol, and register it into the scope. Then for "cls" and
    /// "sup" scopes, add the "Self" symbol.
    auto BindInstance(
      Scope &scope, Vec<TypeId> const &args_ids,
      Scope *self_of, ScopeManager *sm,
      meta::CompilerMetaData *meta) -> void {
      for (const auto args_id : args_ids) {
        // Extract the interned argument ids as a Type id, into a
        // vector of "TypeIdArg" objects.
        const auto args = scopes::ArgsOf(args_id);

        for (auto const &arg : args) {
          if (not arg.Named) { Ice(*scope.AstNode, *sm->CurrentScope, "Unnamed generic argument at instantiation"); }
        }

        // Create a new generic type symbol per generic type arg,
        // and register it into the scope.
        for (auto const &arg : args) {
          if (arg.TypeVal == nullptr) { continue; }
          const auto name = BindingTypeName(arg);
          RegisterGnTypeSymbol(
            scope, CreateGnTypeSymbol(name, arg.TypeVal, *sm, *meta));
        }

        // Create a new generic comp symbol per generic comp arg,
        // and register it into the scope.
        for (auto const &arg : args) {
          if (arg.CompVal == nullptr) { continue; }
          const auto name = BindingCompName(arg);
          const auto param = scope.FindVarSymbol(name.get(), true);
          RegisterGnCompSymbol(
            scope, CreateGnCompSymbol(name, arg.CompVal, *sm, meta, param));
        }
      }

      // Finally add the "Self" symbol if required.
      if (self_of != nullptr) {
        scope.AddTypeSymbol(
          self_type::MakeSelfTypeSymbol(self_of, &scope, 0));
      }
    }

    /**
     * Analyse a type a substitution has just produced. These types are reached out of the order the writer's own types
     * are, so the checks that assume that order are relaxed for them: an instantiation may name an abstract type before
     * the implementation that satisfies it is attached, and a "sup" block's super class is as visible from the
     * instantiation as it was from the template. The relaxations only ever loosen what the caller already allows.
     *
     * @param type The substituted type to analyse.
     * @param tm The scope manager to analyse it through.
     * @param meta The compiler meta data.
     * @param ignore_access Whether access modifiers are ignored.
     */
    auto AnalyseSubstitutedType(
      TypeAst &type,
      ScopeManager *tm,
      meta::CompilerMetaData *meta,
      const bool ignore_access)
      -> void {
      const auto _meta_guard = meta::MetaGuard(meta);
      meta->AllowAbstractType = true;
      meta->IgnoreAccessModifierViolations = meta->IgnoreAccessModifierViolations or ignore_access;
      meta->SkipSubstitutedConstraintChecks = true;
      type.Stage7_AnalyseSemantics(tm, meta);
    }

    /// Read the types that the variable symbols on the type
    /// carry (the class's fields as variable symbols -> their
    /// types), through the generic bindings.
    auto ReadVarSymbolTypes(
      Scope const &scope, ScopeManager *tm, meta::CompilerMetaData *meta,
      TypeAst const *self_type, const bool is_cls = false,
      scopes::GenericSubst const &class_args = {}) -> void {
      // "Self" is parameter 0, bound beside the class's own: "self: Self" in "sup [cmp w: U32] SizedInteger[w,
      // false]" is "SizedInteger[w, false]", then "SizedInteger[64_u32, false]"; "Vec[Self]" in "Node[S32]" is
      // "Vec[Node[S32]]". The template resolves its own in stage 7, so a copy taken earlier - a comp expression in a
      // signature, checked in stage 6 - still holds the bare "Self", which means nothing outside it.

      // A class's types are read through its own scope's bindings, so only anything else needs these.
      auto bindings = class_args;
      if (not is_cls and self_type != nullptr) { scopes::BindSelf(bindings, *self_type, scope); }

      // Iterate each variable symbol on the type (these are the
      // class's fields as variable symbols). They will have
      // their type assigned from the (analysed) class's field's
      // type asts. Ignore variable symbols from "cmp" generics.
      for (auto const &scoped_sym : scope.GetAllVarSymbols(true)) {
        if (scoped_sym->Type == nullptr) { continue; }

        // A class's own copy of each type, read through its own
        // scope ("Self" included); anything else's read through
        // its bindings.
        if (is_cls) {
          scoped_sym->Type = AstCloneShared(scoped_sym->Type);
        }
        else {
          scoped_sym->Type = type_resolution::ReadType(
            *scoped_sym->Type, ExprSubst::In(scope, bindings));
        }
        if (meta->CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
          // Note: DO NOT inline "analysed_type", because the scoped_sym->Type
          // can change during the analysis that uses it, leaving the original
          // dereference pointing to garbage.
          const auto analysed_type = scoped_sym->Type;
          AnalyseSubstitutedType(*analysed_type, tm, meta, false);
        }
      }
    }

    /// [CHECKED]
    /// Create the scope of a generic substitution of a function,
    /// bound to the arguments whose identity is "args_id"
    /// ("Scope::ArgsIdOf"); the caller files it against the base
    /// function.
    auto CreateGnFnScope(
      Scope const &old_fun_scope, const TypeId args_id,
      ScopeManager const *sm, meta::CompilerMetaData *meta)
      -> Unique<Scope> {
      // Create the new scope for the generic instantiation of the
      // function's generic base (template) scope.
      auto new_fun_scope = CopyTemplateScope(old_fun_scope, 1uz, true);
      const auto new_fun_scope_ptr = new_fun_scope.get();

      // Bind the generic parameters, against the instantiation
      // itself.
      auto tm = sm->NewManagerAt(new_fun_scope_ptr);
      BindInstance(*new_fun_scope_ptr, {args_id}, nullptr, &tm, meta);

      // Read what the copy inherited through the bindings: the
      // root's symbols, then the function scope's. "Self" means
      // one thing across the instantiation, so it is looked up
      // once, from the root. This is for the wrapping sup scope.
      const auto self_type = new_fun_scope_ptr->FindEnclosingSelfType(*meta);
      ReadVarSymbolTypes(*new_fun_scope_ptr, &tm, meta, self_type.get());

      // Remove the old bindings from the cloned function scope
      // so the new type ids line up (re-add them afterwards in
      // the following block.
      auto &fn_scope = *new_fun_scope_ptr->Children[0];
      for (auto const &arg : scopes::ArgsOf(args_id)) {
        if (arg.TypeVal != nullptr) { ignore = fn_scope.RemTypeSymbol(BindingTypeName(arg).get()); }
        else { ignore = fn_scope.RemVarSymbol(BindingCompName(arg).get()); }
      }
      auto stm = sm->NewManagerAt(&fn_scope);
      ReadVarSymbolTypes(fn_scope, &stm, meta, self_type.get());
      return new_fun_scope;
    }

    thread_local auto instantiation_depth = 0uz;

    /// [CHECKED]
    /// How many nodes "id" is as syntax, which spells each
    /// repeated part out again where its interned key shares
    /// it: "(T, T)" doubled "d" times is 2^d leaves, but "d"
    /// keys. Counted on the keys ("memo"), so linear in them;
    /// stops counting at "cap". Todo: Does this need some
    ///  sort of "comp" inspection as some comp expressions
    ///  might contain generics (object instantiation etc)?
    auto SpelledSizeOf(
      const scopes::TypeId id, const std::size_t cap,
      Map<scopes::TypeId, std::size_t> &memo) -> std::size_t {
      using namespace spp::analyse::scopes;

      // Use a memoised result if it exists to save on running
      // the function again when there's no need to.
      if (id == nullptr) { return 0uz; }
      if (const auto hit = memo.find(id); hit != memo.end()) { return hit->second; }

      // An argument list ("Arg") is its own arguments; anything
      // else is its head and the arguments it holds.
      auto const &head = HeadOf(id);
      const auto args = head.Kind == TypeKey::Tag::Arg
        ? id
        : head.Args;

      // Recursively measure the size of the arguments by number
      // of nodes within the id being inspected.
      auto size = 1uz;
      if (args != nullptr) {
        for (auto const &arg_id : ArgsOf(args)) {
          size += arg_id.TypeVal != nullptr ? SpelledSizeOf(arg_id.TypeVal, cap, memo) : 1uz;
          if (size >= cap) { break; }
        }
      }

      // For variants, check their members too, adding the found
      // size into the accumulation.
      for (const auto member : head.Members) {
        if (size >= cap) { break; }
        size += SpelledSizeOf(member, cap, memo);
      }

      // Memoise the size against the id, and return the size.
      // Todo: maybe make this memoization a bit longer lasting?
      size = std::min(size, cap);
      memo.emplace(id, size);
      return size;
    }

    /// [CHECKED]
    /// The limit of the instantiation depth that generics can
    /// cause, for example to stop an infinite vector of vectors
    /// being instantiated recursively.
    constexpr auto kMaxFnInstantiationDepth = 64uz;

    /// [CHECKED]
    /// The limit of the number of parts/arguments in a generic
    /// instantiation, to prevent infinite tuple expansion from
    /// exhausting compiler memory.
    constexpr auto kMaxFnInstantiationSize = 256uz;

    /// [CHECKED]
    /// Compile-time detection of template recursion to prevent a
    /// memory overload within the compiler. For example, "f[T]"
    /// asking for "f[Vec[T]]" or "f[(T, T)]" gets recursively deeper.
    /// Refuse if the recursion gets too deep, or the arguments
    /// get too large.
    auto EnforceFnInstantiationBounded(
      FunctionPrototypeAst const &tmpl,
      FunctionPrototypeAst::GenericSubstitution const *requested_by,
      const TypeId args_id) -> std::size_t {
      // Walk up the request chain until we reach an instance of
      // the same template, and take its depth (+1 for this
      // instance). This counts how many instances of the template
      // are nested in the chain, so for "f[T] -> g -> f[Vec[T]]",
      // "f" now has a depth of 2 (moved through entire chain).
      auto depth = 1uz;
      for (auto r = requested_by; r != nullptr; r = r->RequestedBy) {
        if (r->Proto->GetNonGnImpl() == &tmpl) {
          depth = r->Depth + 1;
          break;
        }
      }

      // Raise an error if we have exceeded the maximum generic
      // instantiation depth for a function.
      spp::RaiseIf<errors::SppGenericInstantiationDepthError>(
        depth > kMaxFnInstantiationDepth,
        ERR_ARGS(*tmpl.Name));

      // Raise an error if we have exceeded the number of parts
      // within the generic args id, which prevents infinite
      // tuple expansions etc.
      auto memo = spp::Map<scopes::TypeId, std::size_t>();
      spp::RaiseIf<errors::SppGenericInstantiationDepthError>(
        SpelledSizeOf(args_id, kMaxFnInstantiationSize, memo) >= kMaxFnInstantiationSize,
        ERR_ARGS(*tmpl.Name));

      // Return the depth so it can be bound into the generic
      // substitution of the function, for further tracking like
      // the usage above.
      return depth;
    }

    /// What a read makes an instantiation through, while it can ("StartInstantiatingOnRead"): no tree is none.
    struct InstantiatingOnRead {
      spp::analyse::scopes::ScopeTree *Tree = nullptr;
      spp::asts::meta::CompilerStage Stage = spp::asts::meta::CompilerStage::kPreAnalyseSemantics;
    };

    auto _OnRead = InstantiatingOnRead();
  }
}

SPP_MOD_BEGIN
spp::analyse::utils::monomorphization::InstantiationDepth::InstantiationDepth(
  TypeAst const &at) {
  ++instantiation_depth;
  if (instantiation_depth > 128) {
    --instantiation_depth;
    Raise<errors::SppGenericInstantiationDepthError>(ERR_ARGS(at));
  }
}

spp::analyse::utils::monomorphization::InstantiationDepth::~InstantiationDepth() {
  --instantiation_depth;
}

SPP_MOD_END

auto spp::analyse::utils::monomorphization::StartInstantiatingOnRead(
  ScopeTree &tree, const meta::CompilerStage stage) -> void {
  _OnRead = InstantiatingOnRead{.Tree = &tree, .Stage = stage};
}

auto spp::analyse::utils::monomorphization::StopInstantiatingOnRead() -> void {
  _OnRead = InstantiatingOnRead();
}

auto spp::analyse::utils::monomorphization::InstantiateForScope(
  const TypeId id, Scope const &scope)
  -> TypeSymbol* {
  if (_OnRead.Tree == nullptr) { return nullptr; }

  // Made already under that identity: nothing to make.
  if (const auto made = scope.FindTypeSymbolById(id); made != nullptr) { return made; }

  // The name the identity stands for, read in the global scope, where the instantiation is made, as the stage making
  // it would make it.
  const auto type = scope.TypeAstOf(id);
  if (type == nullptr) { return nullptr; }
  auto tm = ScopeManager::ForTree(*_OnRead.Tree);
  auto stage_meta = meta::CompilerMetaData();
  stage_meta.CurrentStage = _OnRead.Stage;
  auto *const meta = &stage_meta;

  // A class's instance is made from its identity: its template and arguments are already read, named and solved, so
  // nothing is looked up, named, solved or keyed again. Only the arguments are analysed (where "tm" reads them, as the
  // instance binds them), with checks relaxed as for any substituted type ("AnalyseSubstitutedType").
  auto const &head = scopes::HeadOf(id);
  auto *const tmpl = head.Kind == TypeKey::Tag::Inst ? head.Symbol() : nullptr;
  auto &name = *type->LastTypePart();
  const auto all_named = genex::all_of(name.GnArgGroup->Args, [](auto const &arg) {
    return arg->IsTypeArg() ? arg->KeywordName() != nullptr : arg->CompNameAsId() != nullptr;
  });
  if (tmpl != nullptr and tmpl->Alias == nullptr and tmpl->Kind == TypeKind::Cls and tmpl->Type != nullptr) {
    const auto is_tuple = type_predicates::IsTypeTuple(
      TypeRef::ForKindCheck(*tmpl, *tm.CurrentScope), *tm.CurrentScope);
    if (all_named or is_tuple) {
      const auto _depth = InstantiationDepth(name);
      const auto _meta_guard = meta::MetaGuard(meta);
      meta->AllowAbstractType = true;
      meta->IgnoreAccessModifierViolations = true;
      meta->SkipSubstitutedConstraintChecks = true;
      meta->TypeAnalysisTypeScope = nullptr;
      name.GnArgGroup->Stage7_AnalyseSemantics(&tm, meta);
      return FindOrMakeClsInstance(name, tmpl->SharedFromThis<TypeSymbol>(), id, is_tuple, &tm, meta);
    }
  }

  // An alias's arguments bind its own parameters, and a variant is normalised as it is analysed: the name is analysed
  // as one written there, and made under the identity that gives it. Its written identity is cleared, so it is
  // resolved rather than read back.
  type->StampTypeId(nullptr);
  name.StampTypeId(nullptr);
  AnalyseSubstitutedType(*type, &tm, meta, true);
  return scope.FindTypeSymbolById(name.StampedTypeId());
}

/// [CHECKED]
auto spp::analyse::utils::monomorphization::MonomorphiseToFixedPoint(
  ScopeManager *sm, meta::CompilerMetaData *meta) -> void {
  // Drain the list of functions who own monomorphised versions,
  // and generate them, allowing LLVM IR to be created in the
  // following stages. This is a fixed point drain so all comp-
  // folding is handled beforehand.
  // Taken in the order they were recorded, so an error in one
  // instantiation is reported against the call for it rather
  // than an unrelated later one. Membership is dropped as the
  // entry is taken, not when it was recorded, so a template
  // instantiated again while it is being drained is recorded
  // afresh rather than skipped. Released once drained, so a
  // later compilation in the same process starts empty.
  while (_Cursor < _Pending.size()) {
    const auto fn_template = _Pending[_Cursor++];
    _Queued.erase(fn_template);
    fn_template->AnalysePendingGnSubstitutions(sm, meta);
  }
  ClearInstantiations();
  sm->RestartWalk();
}

auto spp::analyse::utils::monomorphization::FindOrMakeClsInstance(
  TypeIdentifierAst &type_part, Shared<TypeSymbol> const &old_cls_sym,
  const TypeId args_id, const bool is_tuple, ScopeManager *sm,
  meta::CompilerMetaData *meta) -> TypeSymbol* {
  // Made already under that identity: nothing to make.
  if (const auto made = sm->CurrentScope->FindTypeSymbolById(args_id); made != nullptr) { return made; }

  // An alias whose target is one of its own parameters ("type
  // Id[T] = T") has no class behind it to copy: an instantiation
  // of it is its target under the arguments ("Id[Bool]" is "Bool").
  // Not a workaround, just a bit of a messy way it has to be done.
  if (old_cls_sym->Alias != nullptr) {
    const auto written = old_cls_sym->AliasTargetId(nullptr);
    if (written != nullptr and scopes::HeadOf(written).Kind == scopes::TypeKey::Tag::TypeParam) {
      const auto target = aliases::InstanceTargetIdOf(*old_cls_sym, args_id);
      const auto spelled = target != nullptr ? sm->CurrentScope->TypeAstOf(target) : nullptr;
      if (spelled == nullptr) {
        Ice(*old_cls_sym->Name, *sm->CurrentScope, "an instance of a generic alias has no target");
      }
      spelled->Stage7_AnalyseSemantics(sm, meta);
      return sm->CurrentScope->FindTypeSymbol(spelled.get());
    }
  }

  // 0. Stamp the arguments with what they mean where they are
  // written, before anything below copies them.
  for (auto const *arg : type_part.GnArgGroup->GetTypeArgs()) {
    type_resolution::StampTypeParts(*arg->TypeVal, *sm->CurrentScope);
  }
  for (auto const *arg : type_part.GnArgGroup->GetCompArgs()) {
    type_resolution::StampCompParts(*arg->CompVal, *sm->CurrentScope);
  }

  // 1. Copy the template's scope alone (no descendants), named
  // as the instantiated type rather than the template.
  const auto old_cls_scope = old_cls_sym->LinkedScope
    ? old_cls_sym->LinkedScope
    : old_cls_sym->ScopeDefinedIn;
  const auto name_clone = AstCloneShared(&type_part);

  // The clone names the instantiation; it is not written where
  // the instantiation is used, so it answers no access check
  // there - a private "Box[S32]" reached inside std's
  // "Vec[Box[S32]]" body is not std naming "Box".
  name_clone->ClearSourceWritten();

  // Recorded in terms that mean the same wherever it is read ("RecordClosedBindings"). The use site keeps its own node,
  // resolved on read. The clone's arguments may have changed, so it keeps no written identity.
  // A closed instance is shared by every spelling of it, so it is named by its identity's arguments, which mean the
  // same wherever they are read; an open one's name is in its own terms already. One written over a pack's name stays
  // as written: a block over a pack binds it to the empty tuple standing in for every element set, so its identity
  // looks closed.
  const auto names_pack = genex::any_of(type_part.GnArgGroup->Args, [sm](auto const &arg) {
    return packs::DoesArgNameAPack(*arg, *sm->CurrentScope);
  });
  const auto closed = not names_pack and scopes::IsClosedTypeId(args_id);
  const auto built = closed ? sm->CurrentScope->TypeAstOf(args_id) : nullptr;
  if (built != nullptr) {
    // Each pointing where it was written, so an error over one points there.
    auto group = AstClone(built->LastTypePart()->GnArgGroup);
    for (auto const &arg : group->Args) {
      auto const *const written = arg->KeywordName() != nullptr
        ? name_clone->GnArgGroup->At(arg->KeywordName()->ToString().c_str())
        : nullptr;
      if (arg->IsTypeArg() and written != nullptr and written->IsTypeArg()) {
        arg->TypeVal = arg->TypeVal->WithSourceSpanOf(*written->TypeVal);
      }
    }
    name_clone->GnArgGroup = std::move(group);
  }
  else { RecordClosedBindings(*name_clone->GnArgGroup, *sm->CurrentScope); }
  name_clone->StampTypeId(nullptr);

  /** SCOPE LOGIC **/

  // Copied alone, and named as the instantiation; it keeps
  // the template's own non-generic scope: the class itself,
  // or for an alias, the class the alias names.
  auto new_cls_scope = CopyTemplateScope(*old_cls_scope, 0uz, false);
  const auto new_cls_scope_ptr = new_cls_scope.get();
  new_cls_scope_ptr->Name = ScopeTypeIdentifierName(name_clone);

  /** SYMBOL LOGIC **/

  // Create a new class symbol, based on the new class scope,
  // and copy over important information, including a property
  // derivation link. Then link the template/instances together
  // at the symbol level.
  const auto new_cls_sym = MakeShared<TypeSymbol>(
    name_clone, AstAs<ClassPrototypeAst>(new_cls_scope->AstNode),
    new_cls_scope.get(), sm->CurrentScope, old_cls_sym->Kind,
    old_cls_sym->IsDirectlyCopyable, old_cls_sym->Visibility);
  new_cls_sym->DerivesFromSymbol = old_cls_sym;
  new_cls_sym->InstanceOf = old_cls_sym.get();
  new_cls_sym->Id = args_id;
  if (const auto tmpl = scopes::HeadOf(args_id).Symbol(); tmpl != nullptr) {
    tmpl->Instances[scopes::BareOf(args_id)] = new_cls_sym.get();
  }
  new_cls_sym->IsConcrete = scopes::IsConcreteTypeId(args_id);
  new_cls_scope_ptr->LinkedTypeSymbol = new_cls_sym;

  /** ALIAS LOGIC **/

  // An instantiation of a generic alias is the same alias under
  // substituted arguments.
  auto new_alias = Shared<AliasInfo>(nullptr);
  if (old_cls_sym->Alias != nullptr) {
    new_alias = MakeShared<AliasInfo>(*old_cls_sym->Alias); // Copy
    const auto target = aliases::InstanceTargetIdOf(*old_cls_sym, args_id);
    new_alias->Resolved = target != nullptr ? sm->CurrentScope->TypeAstOf(target) : nullptr;
    if (new_alias->Resolved == nullptr) {
      Ice(*old_cls_sym->Name, *sm->CurrentScope, "an instance of a generic alias has no target");
    }
    new_alias->Resolved->Stage7_AnalyseSemantics(sm, meta);
    // TODO: Remove generic parameters that have been given arguments (not always all generic args)?
  }

  /** SCOPE TREE LOGIC **/

  // 2. Attach the instantiation to the scope tree. An aliased
  // type is attached where the alias was written, so that the
  // alias and the type it maps to are reachable from each
  // other; anything else sits beside its own template.
  if (new_alias != nullptr) {
    new_alias->DeclaredIn()->AddTypeSymbol(new_cls_sym);
    new_alias->TrackingScope->AddTypeSymbol(new_cls_sym);
    new_alias->TrackingScope->Children.emplace_back(std::move(new_cls_scope));
    new_cls_sym->Alias = std::move(new_alias);
  }
  else {
    new_cls_scope_ptr->Parent->AddTypeSymbol(new_cls_sym);
    new_cls_scope_ptr->Parent->Children.emplace_back(std::move(new_cls_scope));
  }

  // Run the sup scope attachment logic if we are at the stage
  // where that is applicable.
  if (meta->CurrentStage >= meta::CompilerStage::kAttachSupScopes) {
    sup_attachment::AttachSpecificSuperScopes(*new_cls_scope_ptr, *sm, meta);
  }

  /** AST LOGIC **/

  // Register the instantiation's own ast against the template.
  // Its parameter list is emptied, which is what marks it as
  // an instantiation rather than the template it was cloned from.
  auto new_ast = AstClone(AstAs<ClassPrototypeAst>(old_cls_scope->AstNode));
  new_ast->SetAstScope(new_cls_scope_ptr);
  new_ast->GnParamGroup->Params.clear();
  const auto new_ast_ptr = new_ast.get();
  old_cls_sym->Type->AddGnSubstitution(
    {.InstanceScope = new_cls_scope_ptr, .Proto = std::move(new_ast)});

  // No more work for tuples. This is needed to prevent recursive
  // checks on the generics (variadics becoming tuples infinitely).
  if (is_tuple) { return new_cls_sym.get(); }

  /** GENERIC INJECTION LOGIC **/

  // Determine the argument lists that the new instance's generic
  // parameters get bound to. This is a "list of generic lists",
  // and the initial list is either the instantiation's args, or
  // (for a variant), the type's written args. Todo: Better way?
  auto const &head = scopes::HeadOf(args_id);
  auto bound_args = Vec<TypeId>{
    head.Kind == TypeKey::Tag::Inst
    ? head.Args
    : sm->CurrentScope->ArgsIdOf(type_part.GnArgGroup->GetAllArgs(), old_cls_sym->GnParams())
  };

  // For an alias, add another layer of generics into the
  // bound list, which focus on the type statements' generic
  // mapping from parameters into the rhs arguments.
  if (auto const &alias = new_cls_sym->Alias; alias != nullptr and alias->Resolved != nullptr) {
    const auto target_id = alias->Resolved->LastTypePart()->StampedTypeId();
    if (target_id != nullptr and scopes::HeadOf(target_id).Kind == TypeKey::Tag::Inst) {
      const auto args = scopes::HeadOf(target_id).Args;
      if (genex::all_of(scopes::ArgsOf(args), [](auto const &arg) { return arg.Named; })) {
        bound_args.emplace_back(args);
      }
    }
  }

  // Bind all the args into the type and its scope.
  BindInstance(
    *new_cls_scope_ptr, bound_args, new_cls_scope_ptr, sm, meta);

  // Todo: an aliased instantiation arguably wants its substituted types read through the scope the alias maps onto,
  //  rather than its own. The branch that did that was dead (it tested a unique_ptr that had already been moved into
  //  the symbol), so it is left out here rather than silently switched on.

  // 4. Read what the clone inherited through the bindings
  // the attribute symbols, and the attribute asts they came
  // from.
  auto tm = sm->NewManagerAt(new_cls_scope_ptr);
  ReadVarSymbolTypes(*new_cls_scope_ptr, &tm, meta, nullptr, true);

  if (meta->CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
    for (const auto attr : new_ast_ptr->Impl->Members
         | genex::views::ptr
         | genex::views::cast_dynamic<ClassAttributeAst*>()) {
      // An attribute holds a value, so is never abstract, even
      // when the instantiation is reached from somewhere that
      // allows naming one ("B[C[A]]" instantiates "C[A]" while
      // "B"'s arguments are being analysed).
      const auto _meta_guard = meta::MetaGuard(meta);
      meta->AllowAbstractType = false;
      attr->Stage7_AnalyseSemantics(&tm, meta);
    }
  }

  return new_cls_sym.get();
}

auto spp::analyse::utils::monomorphization::MakeSupInstance(
  Scope &old_sup_scope, Scope &new_cls_scope, const TypeId args_id,
  ScopeManager const *sm, meta::CompilerMetaData *meta) -> Tup<Scope*, Scope*> {
  // 1. Clone the template's scope, two levels down: each member function's mock "sup $M ext FunXxx" block, and the
  // function's own scope below it, so each member has a copy under the instantiation, resolving through its bindings;
  // but only the block's own symbols are made its own. Not the members' bodies: the members are functions, each
  // instantiated in its own right by "CreateGnFnScope" when it is called, which analyses its body afresh.
  auto new_sup_scope = CopyTemplateScope(old_sup_scope, 2uz, false);
  auto *const new_sup_scope_ptr = new_sup_scope.get();

  // A "cmp" on a generic "sup" block mangles to "<module>#<name>": "MangleModName" drops every "<...>" scope, so
  // neither the block nor its generic arguments reach the name, and one written constant is one global however many
  // instantiations name it. The copy above therefore has to be undone for the llvm record specifically, or stage 10 -
  // which only ever walks the template - would give storage to a symbol that "Atom[Bool]::mo_release" never resolves
  // to. Point both symbols at the one record, so the global emitted for the template is the one an instantiation loads.
  for (auto const &scoped_sym : new_sup_scope_ptr->GetAllVarSymbols(true)) {
    if (const auto old_sym = old_sup_scope.FindVarSymbol(scoped_sym->Name.get(), true); old_sym != nullptr) {
      scoped_sym->LlvmInfo = old_sym->LlvmInfo;
    }
  }

  // A method's "$" mock was copied with the block's symbols, so the copy is linked to its own cloned scope, and given
  // the cloned "sup $M ext FunXxx" blocks, which read the function type with this instantiation's bindings.
  for (auto i = 0uz; i < old_sup_scope.Children.size() and i < new_sup_scope_ptr->Children.size(); ++i) {
    auto const &old_child = *old_sup_scope.Children[i];
    auto &new_child = *new_sup_scope_ptr->Children[i];
    if (old_child.LinkedTypeSymbol == nullptr or old_child.LinkedTypeSymbol->Kind != TypeKind::FnMock) { continue; }
    auto *const own = new_sup_scope_ptr->FindTypeSymbol(old_child.LinkedTypeSymbol->Name.get(), true);
    if (own == nullptr or own == old_child.LinkedTypeSymbol.get()) { continue; }
    own->LinkedScope = &new_child;
    new_child.LinkedTypeSymbol = own->SharedFromThis<TypeSymbol>();
  }
  for (auto const &child : new_sup_scope_ptr->Children) {
    const auto ext = AstAs<SupPrototypeExtensionAst>(child->AstNode);
    if (ext == nullptr or not ext->Name->IsCompilerGeneratedType()) { continue; }
    if (auto *const mock = child->FindTypeSymbol(ext->Name.get()); mock != nullptr and mock->LinkedScope != nullptr
      and mock->LinkedScope->Parent == new_sup_scope_ptr) {
      sm->Tree().NormalSupBlocks[mock].emplace_back(child.get());
    }
  }

  // 2. Attach the instantiation to the scope tree, beside the template it was cloned from.
  old_sup_scope.Parent->Children.emplace_back(std::move(new_sup_scope));

  // 3. Bind the generic parameters, against the instantiation itself.
  auto tm = sm->NewManagerAt(new_sup_scope_ptr);
  BindInstance(*new_sup_scope_ptr, {args_id}, &new_cls_scope, &tm, meta);

  // 4. Substitute the bindings into what the clone inherited: the block's "type" aliases and its "cmp" constants.
  for (auto const &scoped_sym : new_sup_scope_ptr->GetAllTypeSymbols(true)) {
    if (scoped_sym->Alias == nullptr) { continue; }
    auto old_type_sub = type_resolution::ReadType(*scoped_sym->Alias->Resolved, ExprSubst::In(*new_sup_scope_ptr));
    const auto old_type_sub_sym = TypeRef::Of(*old_type_sub, *new_sup_scope_ptr).Symbol;

    // Its own description rather than the one it was cloned holding: that one still describes the template, and is
    // shared with it, so substituting into it in place would rewrite the template's own meaning.
    auto substituted = MakeShared<AliasInfo>(*scoped_sym->Alias);
    substituted->Resolved = std::move(old_type_sub);
    substituted->WrittenIn = new_sup_scope_ptr;
    scoped_sym->Alias = std::move(substituted);

    if (old_type_sub_sym != nullptr) {
      // Stamped with what it resolved to here, as the template's target is ("TypeStatementAst::Stage4_ResolveDeclarations").
      auto const &resolved = scoped_sym->Alias->Resolved;
      type_resolution::StampType(*resolved, *old_type_sub_sym);
      scoped_sym->LinkTo(*old_type_sub_sym);
    }
  }
  // The class's own parameters, which a template's "Self" names, are the instance's arguments.
  const auto class_args = scopes::InstanceBindings(TypeRef::ForKindCheck(new_cls_scope));
  ReadVarSymbolTypes(*new_sup_scope_ptr, &tm, meta, nullptr, false, class_args);

  // Create the scope for the new super class type. This will handle recursive sup-scope creation.
  auto super_cls_scope = static_cast<Scope*>(nullptr);
  if (const auto ext_ast = AstAs<SupPrototypeExtensionAst>(old_sup_scope.AstNode); ext_ast != nullptr) {
    const auto new_fq_super_type = type_resolution::ReadType(*ext_ast->SuperCls, ExprSubst::In(*new_sup_scope_ptr));
    AnalyseSubstitutedType(*new_fq_super_type, &tm, meta, true);

    // Resolved where it was analysed: a super class naming a generic the instantiation carries is registered with
    // that generic, which the class being attached to has no path to.
    const auto super_cls_sym = TypeRef::Of(*new_fq_super_type, *new_sup_scope_ptr).Symbol;
    super_cls_scope = super_cls_sym != nullptr ? super_cls_sym->LinkedScope : nullptr;
  }

  return {new_sup_scope_ptr, super_cls_scope};
}

auto spp::analyse::utils::monomorphization::FindOrMakeFnInstance(
  FunctionPrototypeAst *fn_proto, Scope const *fn_scope, const TypeId args_id,
  Shared<TypeAst> const &variadic_pack_type, ScopeManager *sm,
  meta::CompilerMetaData *meta) -> Tup<FunctionPrototypeAst*, Scope const*> {
  //
  using errors::SppSecondClassBorrowViolationError;
  using type_predicates::IsTypeBorrowed;

  // Filed under what its arguments mean at the call, excluding
  // any restating its own parameter, and with the variadic
  // pack's tuple; nothing left is the template itself.
  const auto id = CallArgsId(*fn_proto, args_id, variadic_pack_type, sm->CurrentScope);
  if (id != nullptr) {
    // If we have already filed a substitution for the parsed
    // args id, return/reuse that cached prototype/scope.
    if (const auto existing = fn_proto->FindGnSubstitution(id); existing != nullptr) {
      return {existing->Proto.get(), existing->WalkScope()};
    }

    // Run the recursion checks to ensure we aren't heading
    // towards an infinite depth or generic expansion in
    // analysis (compiler OOM).
    const auto requested_by = FunctionPrototypeAst::AnalysingGnSubstitution();
    const auto depth = EnforceFnInstantiationBounded(
      *fn_proto, requested_by, id);

    // Clone the function prototype's signature (not body),
    // set the non generic pointer, and detach the llvm
    // slot (sets it to nullptr).
    auto new_fn_proto = fn_proto->CloneSignature();
    new_fn_proto->SetNonGnImpl(fn_proto);
    new_fn_proto->DetachLlvmFnSlot();

    // Create the new function scope for the generic implementation.
    auto new_fn_scope_owned = CreateGnFnScope(*fn_scope, id, sm, meta);
    const auto new_fn_scope = new_fn_scope_owned.get();
    auto tm = sm->NewManagerAt(new_fn_scope);

    // What "Self" is in the instantiation, read once for
    // everything below that retypes by it: what it pins
    // "Self" to (a call's receiver), and what "Self" then
    // means in its scope - the pin, else the owner.
    const auto pin = scopes::FindArgOf(id, 0); // 0 for self
    const auto pinned_self = pin.has_value() and pin->TypeVal != nullptr and pin->TypeVal != scopes::ParamTypeId(0)
      ? sm->CurrentScope->TypeAstOf(pin->TypeVal)
      : nullptr;
    const auto instance_self = new_fn_scope->FindEnclosingSelfType(*meta);

    // Drop only the parameters this substitution actually
    // bound.
    new_fn_proto->GnParamGroup->Params |= genex::actions::remove_if([&](auto const &param) {
      return genex::any_of(scopes::ArgsOf(id), [&](auto const &arg) { return arg.Slot == param->ParamId(); });
    });

    // Get the bindings (generic substitution map) from the
    // call generic args id.
    const auto instance_bindings = scopes::BindingsOfArgs(id);

    // The template's parameters are keyed in its own function
    // scope, which declares its generics and reaches its "sup"
    // block's through its ancestors; one with no scope of its
    // own (an anonymous function) in the scope it was found in.
    auto const &template_scope = fn_proto->GetAstScope() != nullptr
      ? *fn_proto->GetAstScope()
      : *fn_scope;

    // Extract the function parameters on the template of the
    // function, and the instance of the function.
    const auto template_fn_params = fn_proto->FnParamGroup->GetNonSelfParams();
    const auto instance_fn_params = new_fn_proto->FnParamGroup->GetNonSelfParams();
    for (auto i = 0uz; i < instance_fn_params.size(); ++i) {
      // For each instance function parameter, get the type on
      // the parameter, and get the id of it under the template
      // scope (ie where this was originally written).
      const auto p = instance_fn_params[i];
      const auto declared = template_scope.TypeIdOf(*template_fn_params[i]->Type);

      // Run the parameter's type's id through the instance
      // bindings, and stamp it onto the new parameter,
      p->InstanceTypeId = declared != nullptr ? scopes::SubstituteTypeId(declared, instance_bindings) : nullptr;
      p->Type = type_resolution::ReadType(*p->Type, ExprSubst::In(*new_fn_scope));
      p->Type->Stage7_AnalyseSemantics(&tm, meta);

      // The default is checked against the substituted type,
      // so it is rebuilt from what was written, in the same
      // terms.
      const auto opt = p->To<FunctionParameterOptionalAst>();
      if (opt != nullptr and opt->Source.OriginalDefaultVal != nullptr) {
        opt->DefaultVal = AstClone(opt->Source.OriginalDefaultVal->ReadExpr(ExprSubst::In(*new_fn_scope)));
      }
    }

    // The parameters' symbols are what the signature now says. Then a parameter whose type is now "Void" is no
    // parameter of the instantiation; its symbol stays, as the body still names it.
    TypeParamSymbolsFromSignature(*new_fn_proto, *fn_proto, pinned_self, variadic_pack_type, tm, meta);
    new_fn_proto->FnParamGroup->RemoveVoidParams(*new_fn_scope);

    if (const auto declared = template_scope.TypeIdOf(*fn_proto->ReturnType); declared != nullptr) {
      new_fn_proto->InstanceReturnTypeId = scopes::SubstituteTypeId(declared, instance_bindings);
    }
    new_fn_proto->ReturnType = type_resolution::ReadType(*new_fn_proto->ReturnType, ExprSubst::In(*new_fn_scope));
    new_fn_proto->ReturnType->Stage7_AnalyseSemantics(&tm, meta);

    // Check the new return type isn't a borrow type.
    RaiseIf<SppSecondClassBorrowViolationError>(
      IsTypeBorrowed(*new_fn_proto->ReturnType, tm),
      ERR_ARGS(*new_fn_proto->ReturnType, *new_fn_proto->ReturnType, "substituted function return type"));

    const auto is_concrete = ComputeIsConcrete(id, *new_fn_proto, instance_self.get(), tm, meta);

    // File the substitution against the base function, built but
    // not required: nothing is filed until the whole signature has
    // been substituted, so a failure part way leaves no trace.
    auto &sub = fn_proto->AddGnSubstitution(FunctionPrototypeAst::GenericSubstitution{
      .OwnedScope = std::move(new_fn_scope_owned),
      .Proto = std::move(new_fn_proto),
      .IsConcrete = is_concrete,
      .IsBodyAnalysed = false,
      .ArgsId = id,
      .IsRequired = false,
      .RequestedBy = requested_by,
      .Depth = depth
    });
    return {sub.Proto.get(), new_fn_scope};
  }

  return {fn_proto, fn_scope};
}

auto spp::analyse::utils::monomorphization::RequireFnInstance(
  FunctionPrototypeAst *fn_proto,
  Scope const *fn_scope,
  const TypeId args_id,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> FunctionPrototypeAst* {
  // The arguments arrive already named - they are read off a
  // "sup" block that has bound them - so there is nothing here
  // for inference to do, and the substitution itself is the
  // whole of what a call site would reach.
  const auto instance = get<0>(FindOrMakeFnInstance(
    fn_proto, fn_scope, args_id, nullptr, sm, meta));
  instance->RequireGnSubstitution();
  return instance;
}

auto spp::analyse::utils::monomorphization::FindRequiredFnInstance(
  FunctionPrototypeAst *fn_proto,
  const TypeId args_id)
  -> FunctionPrototypeAst* {
  // Nothing to substitute means the template is the only prototype there is, exactly as
  // "FindOrMakeFnInstance" decides it.
  const auto id = CallArgsId(*fn_proto, args_id, nullptr, nullptr);
  if (id == nullptr) { return fn_proto; }
  auto const *const sub = fn_proto->FindGnSubstitution(id);
  return sub != nullptr and sub->IsRequired ? sub->Proto.get() : nullptr;
}

auto spp::analyse::utils::monomorphization::EnqueueInstantiation(
  FunctionPrototypeAst *fn_template) -> void {
  // A template already waiting is left where it is: draining
  // one re-reads its substitution list from scratch, so a
  // single entry covers however many instantiations were
  // registered against it in the meantime.
  if (not _Queued.insert(fn_template).second) { return; }
  _Pending.emplace_back(fn_template);
}

auto spp::analyse::utils::monomorphization::ClearInstantiations() -> void {
  _Pending.clear();
  _Queued.clear();
  _Cursor = 0uz;
}
