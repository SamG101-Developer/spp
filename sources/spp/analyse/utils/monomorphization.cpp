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
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.aliases;
import spp.analyse.utils.packs;
import spp.analyse.utils.self_type;
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
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.codegen.llvm_ctx;
import spp.utils.algorithms;
import spp.utils.interner;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;
import sys;

namespace spp::analyse::utils::monomorphization {
  namespace {
    /// The templates waiting, in the order they were recorded.
    auto _Pending = Vec<FunctionPrototypeAst*>();

    /// The index of the next template to be drained.
    auto _Cursor = 0uz;

    /// Membership of the unconsumed part of the pending templates.
    auto _Queued = Set<FunctionPrototypeAst*>();

    /**
     * Re-record, against the instantiation's own symbols, the callability its parameters inherited from their generic
     * constraints. A parameter declared "F: FunMov" keeps being callable through what the constraint promised even
     * though its type has just been rewritten to the argument: "FunMut" satisfies "FunMov" while also being callable
     * through a borrow, so reading the substituted type would make this body consume the value here and borrow it in
     * the next instantiation, which linear ownership cannot account for. It goes on the symbol because the constraint
     * lives on the template's generic parameter, which nothing in the instantiation refers to any more.
     * @param new_fn_proto The instantiated prototype whose parameter symbols are being retyped.
     * @param fn_proto The template the instantiation was cloned from, which still carries the constraints.
     * @param new_fn_scope The instantiation's scope.
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param meta Associated metadata.
     */
    auto ReattachCallableConstraints(
      FunctionPrototypeAst const &new_fn_proto, FunctionPrototypeAst const &fn_proto,
      Scope *new_fn_scope, ScopeManager &tm, meta::CompilerMetaData *meta) -> void {
      for (auto const *p : new_fn_proto.FnParamGroup->GetNonSelfParams()) {
        const auto declared = p->Source.OriginalType;
        if (declared == nullptr) { continue; }

        const auto gn_param = genex::find_if(
          fn_proto.GnParamGroup->Params, [&](auto const &g) { return *g->Name == *declared; });
        if (gn_param == fn_proto.GnParamGroup->Params.end()) { continue; }

        auto const &constraints = *gn_param;
        if (constraints->TypeConstraints == nullptr) { continue; }

        for (auto const &c : constraints->TypeConstraints->Constraints) {
          if (not type_predicates::IsTypeFunction(TypeRef::ForKindCheck(*c, *new_fn_scope), *new_fn_scope)) {
            continue;
          }
          const auto sym = new_fn_scope->Children[0]->FindVarSymbol(p->ExtractName().get(), true);
          if (sym == nullptr) { break; }

          // Read as the parameter's own type is: the constraint is
          // written in the template's terms ("FunMov[(T,), U]"),
          // and what the call needs is this instantiation's argument
          // and return types.
          auto callable = type_resolution::ReadType(*c, ExprSubst::In(*new_fn_scope));
          callable->Stage7_AnalyseSemantics(&tm, meta);

          // Through an alias, as the name of what it resolves to
          // ("TypeRef" never answers with an alias).
          const auto head = new_fn_scope->FindHeadSymbol(*callable);
          auto const *const full = TypeRef::Of(*callable, *new_fn_scope).Symbol;
          sym->CallableAsType = head != nullptr and head->Alias != nullptr and full != nullptr
            ? AstCloneShared(full->FqName())
            : callable;
          break;
        }
      }
    }

    /**
     * Retype the two symbols an instantiation inherits from its template rather than getting for itself: "self" and
     * the variadic pack. Both are typed by stage 6, which only ever runs on the template, so the clone would otherwise
     * carry the template's symbol - "self" still typed as the template's "Self", and a pack still typed as the single
     * element it declares ("..b: T") rather than the tuple the call collapsed its trailing arguments into.
     * @param new_fn_proto The instantiated prototype, whose "self" parameter and pack type are rewritten in place.
     * @param new_fn_scope The instantiation's scope, holding the symbols to retype.
     * @param pinned_self What the instantiation pins "Self" to (a call's receiver), or @c nullptr .
     * @param variadic_pack_type The tuple the call collapsed its trailing arguments into, or @c nullptr .
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param meta Associated metadata.
     */
    auto RetypeInheritedSymbols(
      FunctionPrototypeAst &new_fn_proto,
      Scope *new_fn_scope,
      Shared<TypeAst> const &pinned_self,
      Shared<TypeAst> const &variadic_pack_type,
      ScopeManager &tm,
      meta::CompilerMetaData *meta)
      -> void {
      // "self" is typed as "Self", so only an instantiation that
      // pins "Self" to the receiver has anything to rewrite.
      if (const auto self_param = new_fn_proto.FnParamGroup->GetSelfParam(); self_param != nullptr) {
        if (pinned_self != nullptr) {
          const auto substituted_self = AstCloneShared(pinned_self);
          substituted_self->Stage7_AnalyseSemantics(&tm, meta);
          self_param->Type = substituted_self;
          const auto self_name = self_param->ExtractName();
          if (const auto self_sym = new_fn_scope->Children[0]->FindVarSymbol(self_name.get(), true);
            self_sym != nullptr) {
            self_sym->Type = substituted_self->WithConvention(AstClone(self_param->Conv));
          }
        }
      }
      new_fn_proto.VariadicPackType = AstClone(variadic_pack_type);

      // A variadic parameter declares one element but binds the
      // whole tuple.
      if (variadic_pack_type != nullptr) {
        auto pack_type = AstClone(variadic_pack_type);
        pack_type->Stage7_AnalyseSemantics(&tm, meta);
        const auto pack_name = new_fn_proto.FnParamGroup->GetVariadicParam()->ExtractName();
        if (const auto pack_sym = new_fn_scope->Children[0]->FindVarSymbol(pack_name.get(), true);
          pack_sym != nullptr) {
          pack_sym->Type = std::move(pack_type);
        }
      }
    }

    /**
     * Whether an instantiation is concrete: every argument it pins names a real type or value, and every type in its
     * own signature resolves to one. A non-concrete instantiation is still a template as far as anything downstream is
     * concerned, so codegen has nothing to emit for it.
     * @param args_id The identity of the arguments this instantiation pins.
     * @param new_fn_proto The instantiated prototype whose signature is checked.
     * @param new_fn_scope The instantiation's scope, which the signature's types are resolved in.
     * @param instance_self What "Self" is in the instantiation, or @c nullptr .
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param meta Associated metadata.
     * @return If the instantiation is fully concrete.
     */
    auto ComputeIsConcrete(
      const TypeId args_id,
      FunctionPrototypeAst const &new_fn_proto,
      Scope const *new_fn_scope,
      TypeAst const *instance_self,
      ScopeManager &tm,
      meta::CompilerMetaData *meta)
      -> bool {
      const auto type_is_concrete = [&](TypeAst const &type) {
        const auto resolved = self_type::SubstituteSelf(type, instance_self, *tm.CurrentScope, &tm, meta);
        return type_predicates::IsTypeConcrete(*resolved, *new_fn_scope);
      };

      // The arguments are asked the same way a class instantiation
      // asks them; a function goes on to check the signature it
      // ended up with, which a class has no equivalent of.
      const auto args_concrete = genex::all_of(scopes::ArgsOf(args_id), [](auto const &arg) {
        return arg.TypeVal != nullptr
          ? scopes::IsConcreteTypeId(arg.TypeVal)
          : scopes::IsConcreteCompId(arg.CompVal);
      });
      return args_concrete
        and type_is_concrete(*new_fn_proto.ReturnType)
        and genex::all_of(new_fn_proto.FnParamGroup->GetAllParams(), [&](auto *p) {
          return type_is_concrete(*p->Type);
        });
    }

    /**
     * The scope a prototype's generic parameters are looked up from: its own function scope, which declares its own
     * and reaches the ones inherited from its "sup" block through its ancestors.
     * @param fn_proto The prototype.
     * @param fallback The scope to use when the prototype has no scope of its own.
     * @return The scope to look the parameters up from.
     */
    auto CalleeScope(
      FunctionPrototypeAst const &fn_proto,
      Scope const &fallback)
      -> Scope const& {
      const auto *own = fn_proto.GetAstScope();
      return own != nullptr ? *own : fallback;
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
      for (auto i = 0uz; i < dst.Children.Len() and i < src.Children.Len(); ++i) {
        GiveScopeOwnSymbols(*dst.Children[i].get(), *src.Children[i].get(), true);
      }
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
     * one spelled from its identity ("ArgNameOf": "Self", a variadic pack's, or a name that is no parameter), which a
     * parameter's records the identity of, as the parameter's own name does.
     */
    auto BindingTypeName(
      scopes::TypeIdArg const &arg)
      -> Shared<TypeIdentifierAst> {
      const auto by_param = arg.Named and not arg.Spelled and arg.Slot != 0;
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
      if (arg.Named and not arg.Spelled and arg.Slot != 0) {
        if (auto const *const param = scopes::FindGnCompParamById(arg.Slot); param != nullptr) { return param->Name; }
      }
      return MakeShared<IdentifierAst>(0, scopes::ArgNameOf(arg));
    }

    /**
     * The identity a call's arguments are filed under ("args_id", keyed at the call): each kept as keyed, less one
     * restating its own parameter - the parameter itself, or what the callee's scope binds it to already, as when one
     * method of an instantiated generic "sup" block calls another - then the variadic pack's tuple, under the pack's own
     * parameter ("GnPackParamId"). Only the same declaration counts, which a parameter's "ParamId" identifies: another
     * context's parameter of the same name is another type, and gets an instantiation of its own. Null when nothing is
     * left, so the call is to the template itself.
     * @param fn_proto The callee.
     * @param callee_scope The scope the callee's generic parameters are looked up from.
     * @param args_id The identity of the call's arguments.
     * @param variadic_pack_type The tuple a variadic parameter's arguments form, if any.
     * @param scope The call site's scope, where the pack's tuple is keyed.
     * @return The identity to file the instantiation under.
     */
    auto CallArgsId(
      FunctionPrototypeAst const &fn_proto,
      Scope const &callee_scope,
      const TypeId args_id,
      Shared<TypeAst> const &variadic_pack_type,
      Scope const &scope)
      -> TypeId {
      const auto restates_own_param = [&](scopes::TypeIdArg const &arg) {
        if (not arg.Named or arg.Spelled or arg.Slot == 0) { return false; }
        if (arg.TypeVal != nullptr) {
          if (arg.TypeVal == scopes::ParamTypeId(arg.Slot)) { return true; }
          auto *const bound = callee_scope.FindTypeSymbol(BindingTypeName(arg).get());
          return bound != nullptr and bound->Kind == TypeKind::GnTypeArg and bound->BindsParamId == arg.Slot
            and TypeRef::Of(*bound, callee_scope, std::nullopt, TypeRef::OnMissing::Null).Id == arg.TypeVal;
        }
        if (arg.CompVal == scopes::ParamCompId(arg.Slot)) { return true; }
        auto const *const bound = callee_scope.FindVarSymbol(BindingCompName(arg).get());
        return bound != nullptr and bound->Kind == VariableKind::GnCompArg and bound->BindsParamId == arg.Slot
          and bound->BoundCompId == arg.CompVal;
      };

      auto key = scopes::TypeKey();
      if (args_id != nullptr) {
        for (auto const &arg : scopes::ArgsOf(args_id)) {
          if (restates_own_param(arg)) { continue; }
          const auto tag = arg.Named and not arg.Spelled
            ? scopes::TypeKey::Tag::Arg
            : arg.Named
            ? scopes::TypeKey::Tag::Name
            : scopes::TypeKey::Tag::Pos;
          key.Push(tag, arg.Slot);
          if (arg.TypeVal != nullptr) { key.PushTypePart(arg.TypeVal); }
          if (arg.CompVal != nullptr) { key.PushCompPart(arg.CompVal); }
        }
      }
      if (variadic_pack_type != nullptr) {
        auto const *const pack = fn_proto.FnParamGroup->GetVariadicParam();
        key.Push(scopes::TypeKey::Tag::Arg, GnPackParamId(pack, packs::PackTypeParamName(*pack->ExtractName())));
        key.PushTypePart(scope.PartialTypeIdOf(*variadic_pack_type));
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
        for (auto i = 0uz; i < packs::CompPackElements(*bound).Len(); ++i) {
          elems.EmplaceBack(AstCloneShared(declared));
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

    /**
     * Bind the generic parameters of an instantiation: register a symbol per argument of "args_id", the identity of its
     * arguments ("Scope::ArgsIdOf"), into the instantiation's scope. Each is read by its identity, which means what it
     * meant where it was keyed; nothing is carried in by spelling.
     * @param args_id The identity of the arguments the generic parameters are being bound to.
     * @param scope The instantiation's scope to register the symbols into.
     * @param sm The scope manager the values are read through.
     * @param meta The compiler meta data.
     */
    auto RegisterGnSymbols(
      const TypeId args_id,
      Scope *scope,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      // An argument is named by now; anything else is a bug.
      using errors::SppInternalCompilerError;
      if (args_id == nullptr) { return; }
      const auto args = scopes::ArgsOf(args_id);
      for (auto const &arg : args) {
        RaiseIf<SppInternalCompilerError>(
          not arg.Named, {sm->CurrentScope}, ERR_ARGS(*scope->AstNode, "Unnamed generic argument at instantiation"));
      }

      // Each kind's symbols are all made before any is registered,
      // so each is read as keyed rather than against a binding made
      // a moment earlier. The comp arguments wait until the type
      // bindings are in: a comp value is typed in this scope, and
      // one naming a type parameter ("cmp p: Box[T]") needs "T"
      // bound by then.
      auto type_syms = Vec<Shared<TypeSymbol>>();
      for (auto const &arg : args) {
        if (arg.TypeVal != nullptr) {
          type_syms.EmplaceBack(CreateGnTypeSymbol(BindingTypeName(arg), arg.TypeVal, *sm, *meta));
        }
      }
      for (auto const &sym : type_syms) { RegisterGnTypeSymbol(*scope, sym); }

      auto comp_syms = Vec<Shared<VariableSymbol>>();
      for (auto const &arg : args) {
        if (arg.CompVal == nullptr) { continue; }
        const auto name = BindingCompName(arg);
        auto const *const param = scope->FindVarSymbol(name.get(), true);
        comp_syms.EmplaceBack(CreateGnCompSymbol(name, arg.CompVal, *sm, meta, param));
      }
      for (auto const &sym : comp_syms) { RegisterGnCompSymbol(*scope, sym); }
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
     * @param allow_abstract Whether naming an abstract type is permitted.
     * @param ignore_access Whether access modifiers are ignored.
     */
    auto AnalyseSubstitutedType(
      TypeAst &type,
      ScopeManager *tm,
      meta::CompilerMetaData *meta,
      const bool allow_abstract,
      const bool ignore_access)
      -> void {
      const auto _meta_guard = meta::MetaGuard(meta);
      meta->AllowAbstractType = meta->AllowAbstractType or allow_abstract;
      meta->IgnoreAccessModifierViolations = meta->IgnoreAccessModifierViolations or ignore_access;
      meta->SkipSubstitutedConstraintChecks = true;
      type.Stage7_AnalyseSemantics(tm, meta);
    }

    /**
     * Make the instance of a class @p tmpl its identity @p id names, without analysing the name: the identity's
     * arguments are already named and solved, so only they are analysed (where @p tm reads them, as the instance binds
     * them), and the instance is made and filed under @p id ("CreateGnClsScope"), as "TypeIdentifierAst::Stage7"
     * would after solving. Checks are relaxed as for any substituted type ("AnalyseSubstitutedType").
     * @param name The name the identity stands for ("Scope::TypeAstOf"), its arguments named.
     * @param tmpl The class template.
     * @param id The instance's identity.
     * @param is_tuple Whether the template is the tuple, whose arguments are positional.
     * @param tm The scope manager the instance is made through.
     * @param meta The compiler meta data.
     * @return The instance's symbol.
     */
    auto MakeInstance(
      TypeIdentifierAst &name,
      TypeSymbol &tmpl,
      const TypeId id,
      const bool is_tuple,
      ScopeManager *tm,
      meta::CompilerMetaData *meta)
      -> TypeSymbol* {
      const auto _depth = InstantiationDepth(name, *tm);
      const auto _meta_guard = meta::MetaGuard(meta);
      meta->AllowAbstractType = true;
      meta->IgnoreAccessModifierViolations = true;
      meta->SkipSubstitutedConstraintChecks = true;
      meta->TypeAnalysisTypeScope = nullptr;
      name.GnArgGroup->Stage7_AnalyseSemantics(tm, meta);
      auto const *const made = CreateGnClsScope(name, tmpl.SharedFromThis<TypeSymbol>(), id, is_tuple, tm, meta);
      return made != nullptr ? made->LinkedTypeSymbol.get() : nullptr;
    }

    /**
     * Read the types the instantiation's variable symbols carry through its bindings. Nothing else re-derives them: a
     * symbol's type is what @c InferType hands back verbatim, so a "that: SizedIntegerUnsigned[w]" left holding the
     * template's own "w" makes the instantiated body infer "w" from "w" and resolve back against the template instead
     * of the instantiation.
     *
     * A class's instantiation ("template_scope" given) reads each attribute from the class's own declaration, through
     * its own scope, whose bindings each parameter is read through, so nothing is substituted. The symbols were copied
     * from whatever it was instantiated from, which for a "use" of the class is the alias's open instance
     * ("Vec[T=ZZ]"), whose types name the alias's parameters instead; and it reads its own copy, never the node it was
     * copied holding, which would leave this instantiation's reading on whatever it came from. Anything else (a
     * function's, a "sup" block's) has its types read through "class_args" ("type_resolution::ReadType").
     * @param scope The instantiation's scope whose own variable symbols are being read.
     * @param tm The scope manager to analyse the read types through.
     * @param meta The compiler meta data.
     * @param self_type What "Self" means in the instantiation, bound as parameter 0 where given.
     * @param template_scope For a class's instantiation, the class's own scope.
     * @param class_args For a "sup" block's instantiation, the class's own parameters bound to the instance it is
     * attached to: a template's "Self" is the class over its own parameters ("Unit[T=T]" is the class's "T", not the
     * block's).
     */
    auto ReadVarSymbolTypes(
      Scope const &scope,
      ScopeManager *tm,
      meta::CompilerMetaData *meta,
      TypeAst const *self_type,
      Scope const *template_scope = nullptr,
      scopes::GenericSubst const &class_args = {})
      -> void {
      // "Self" is parameter 0, bound beside the class's own: "self: Self" in "sup [cmp w: U32] SizedInteger[w,
      // false]" is "SizedInteger[w, false]", then "SizedInteger[64_u32, false]"; "Vec[Self]" in "Node[S32]" is
      // "Vec[Node[S32]]". The template resolves its own in stage 7, so a copy taken earlier - a comp expression in a
      // signature, checked in stage 6 - still holds the bare "Self", which means nothing outside it.
      auto bindings = class_args;
      if (self_type != nullptr) { scopes::BindSelf(bindings, *self_type, scope); }
      for (auto const &scoped_sym : scope.GetAllVarSymbols(true)) {
        if (scoped_sym->Type == nullptr) { continue; }
        if (template_scope != nullptr) {
          auto const *const declared = scoped_sym->Kind == VariableKind::Attribute
            ? template_scope->FindVarSymbol(scoped_sym->Name.get(), true)
            : nullptr;
          scoped_sym->Type = AstCloneShared(declared != nullptr and declared->Type != nullptr
            ? declared->Type
            : scoped_sym->Type);
        }

        if (template_scope == nullptr) {
          scoped_sym->Type = type_resolution::ReadType(*scoped_sym->Type, ExprSubst::In(scope, bindings));
        }
        else if (type_predicates::DoesTypeNameSelf(*scoped_sym->Type)) {
          scoped_sym->Type = self_type::SubstituteSelf(*scoped_sym->Type, self_type, scope);
        }
        if (meta->CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
          // Note: DO NOT inline "analysed_type", because the scoped_sym->Type
          // can change during the analysis that uses it, leaving the original
          // dereference pointing to garbage.
          const auto analysed_type = scoped_sym->Type;
          AnalyseSubstitutedType(*analysed_type, tm, meta, true, false);
        }
      }
    }
  }
}

/// [CHECKED]
namespace {
  thread_local auto instantiation_depth = 0uz;

  /// How many nodes "id" is as syntax, which spells each repeated
  /// part out again where its interned key shares it: "(T, T)"
  /// doubled "d" times is 2^d leaves, but "d" keys. Counted on the
  /// keys ("memo"), so linear in them; stops counting at "cap".
  auto SpelledSizeOf(
    const spp::analyse::scopes::TypeId id, const std::size_t cap,
    spp::Map<spp::analyse::scopes::TypeId, std::size_t> &memo)
    -> std::size_t {
    using namespace spp::analyse::scopes;
    if (id == nullptr) { return 0uz; }
    if (const auto hit = memo.find(id); hit != memo.end()) { return hit->second; }
    // An argument list ("Arg") is its own arguments; anything else
    // is its head and the arguments it holds.
    auto const &head = HeadOf(id);
    const auto args = head.Kind == TypeKey::Tag::Arg ? id : head.Args;
    auto size = 1uz;
    if (args != nullptr) {
      for (auto const &arg : ArgsOf(args)) {
        size += arg.TypeVal != nullptr ? SpelledSizeOf(arg.TypeVal, cap, memo) : 1uz;
        if (size >= cap) { break; }
      }
    }
    for (const auto member : head.Members) {
      if (size >= cap) { break; }
      size += SpelledSizeOf(member, cap, memo);
    }
    size = std::min(size, cap);
    memo.emplace(id, size);
    return size;
  }

  /// How deep a new instantiation of "tmpl" is: one more than the
  /// nearest instantiation of the same template along the chain
  /// that requested it ("RequestedBy"), through however many
  /// others; 1 if none. "f[T]" asking for "f[Vec[T]]" is deeper.
  auto FnInstantiationDepthOf(
    spp::asts::FunctionPrototypeAst const &tmpl,
    spp::asts::FunctionPrototypeAst::GenericSubstitution const *requested_by)
    -> std::size_t {
    for (auto const *r = requested_by; r != nullptr; r = r->RequestedBy) {
      if (r->Proto->GetNonGnImpl() == &tmpl) { return r->Depth + 1; }
    }
    return 1uz;
  }

  /// A recursion of "tmpl" past this many of its own instances is
  /// taken to never end. Below where the nested types it builds
  /// ("Vec[..]") reach "InstantiationDepth"'s 128, so it is reported
  /// on the function rather than inside a type it reached.
  constexpr auto kMaxFnInstantiationDepth = 64uz;

  /// Arguments spelled out past this many nodes ("SpelledSizeOf")
  /// outgrow anything real (which peaks around 20) before they are
  /// deep: "f[(T, T)]" doubles them, and their syntax with them.
  constexpr auto kMaxFnInstantiationSize = 256uz;

  /// Refuse a new instantiation of "tmpl" with arguments "args_id"
  /// at "depth" (E109, on the function) when it is a step of a
  /// recursion that never ends: too deep, or arguments too large.
  auto EnforceFnInstantiationBounded(
    spp::asts::FunctionPrototypeAst const &tmpl, const std::size_t depth,
    const spp::analyse::scopes::TypeId args_id, spp::analyse::scopes::ScopeManager const &sm)
    -> void {
    auto memo = spp::Map<spp::analyse::scopes::TypeId, std::size_t>();
    spp::RaiseIf<spp::analyse::errors::SppGenericInstantiationDepthError>(
      depth > kMaxFnInstantiationDepth
      or SpelledSizeOf(args_id, kMaxFnInstantiationSize, memo) >= kMaxFnInstantiationSize,
      {sm.CurrentScope}, ERR_ARGS(*tmpl.Name));
  }

  /// What a read makes an instantiation through, while it can ("StartInstantiatingOnRead"): no global scope is none.
  struct InstantiatingOnRead {
    spp::Shared<spp::analyse::scopes::Scope> GlobalScope;
    spp::asts::meta::CompilerStage Stage = spp::asts::meta::CompilerStage::kPreAnalyseSemantics;
  };

  auto OnRead() -> InstantiatingOnRead& {
    static auto state = InstantiatingOnRead();
    return state;
  }
}

SPP_MOD_BEGIN
spp::analyse::utils::monomorphization::InstantiationDepth::InstantiationDepth(
  TypeAst const &at, ScopeManager const &sm) {
  ++instantiation_depth;
  if (instantiation_depth > 128) {
    --instantiation_depth;
    Raise<errors::SppGenericInstantiationDepthError>({sm.CurrentScope}, ERR_ARGS(at));
  }
}

spp::analyse::utils::monomorphization::InstantiationDepth::~InstantiationDepth() {
  --instantiation_depth;
}

SPP_MOD_END

auto spp::analyse::utils::monomorphization::StartInstantiatingOnRead(
  Shared<Scope> global_scope, const meta::CompilerStage stage)
  -> void {
  OnRead() = InstantiatingOnRead{.GlobalScope = std::move(global_scope), .Stage = stage};
}

auto spp::analyse::utils::monomorphization::StopInstantiatingOnRead()
  -> void {
  OnRead() = InstantiatingOnRead();
}

auto spp::analyse::utils::monomorphization::InstantiateForScope(
  const TypeId id, Scope const &scope)
  -> TypeSymbol* {
  auto const &on_read = OnRead();
  if (on_read.GlobalScope == nullptr) { return nullptr; }

  // Made already under that identity: nothing to make.
  if (const auto made = scope.FindTypeSymbolById(id); made != nullptr) { return made; }

  // The name the identity stands for, read in the global scope, where the instantiation is made, as the stage making
  // it would make it.
  const auto type = scope.TypeAstOf(id);
  if (type == nullptr) { return nullptr; }
  auto tm = ScopeManager(on_read.GlobalScope, on_read.GlobalScope.get());
  auto stage_meta = meta::CompilerMetaData();
  stage_meta.CurrentStage = on_read.Stage;
  auto *const meta = &stage_meta;

  // A class's instance is made from its identity: its template and arguments are already read, named and solved, so
  // nothing is looked up, named, solved or keyed again.
  auto const &head = scopes::HeadOf(id);
  auto *const tmpl = head.Kind == TypeKey::Tag::Inst ? head.Symbol() : nullptr;
  auto &name = *type->LastTypePart();
  const auto all_named = genex::all_of(name.GnArgGroup->Args, [](auto const &arg) {
    return arg->IsTypeArg() ? arg->KeywordName() != nullptr : arg->CompNameAsId() != nullptr;
  });
  if (tmpl != nullptr and tmpl->Alias == nullptr and tmpl->Kind == TypeKind::Cls and tmpl->Type != nullptr) {
    const auto is_tuple = type_predicates::IsTypeTuple(
      TypeRef::ForKindCheck(*tmpl, *tm.CurrentScope), *tm.CurrentScope);
    if (all_named or is_tuple) { return MakeInstance(name, *tmpl, id, is_tuple, &tm, meta); }
  }

  // An alias's arguments bind its own parameters, and a variant is normalised as it is analysed: the name is analysed
  // as one written there, and made under the identity that gives it. Its written identity is cleared, so it is
  // resolved rather than read back.
  type->StampTypeId(nullptr);
  name.StampTypeId(nullptr);
  AnalyseSubstitutedType(*type, &tm, meta, true, true);
  return scope.FindTypeSymbolById(name.StampedTypeId());
}

/// [CHECKED]
auto spp::analyse::utils::monomorphization::MonomorphiseToFixedPoint(
  ScopeManager *sm, meta::CompilerMetaData *meta) -> void {
  // Drain the list of functions who own monomorphised versions,
  // and generate them, allowing LLVM IR to be created in the
  // following stages. This is a fixed point drain so all comp-
  // folding is handled beforehand.
  while (const auto fn_template = PopInstantiation()) {
    fn_template->AnalysePendingGnSubstitutions(sm, meta);
  }
  sm->Reset();
}

auto spp::analyse::utils::monomorphization::CreateGnClsScope(
  TypeIdentifierAst &type_part,
  Shared<TypeSymbol> const &old_cls_sym,
  const scopes::TypeId args_id,
  const bool is_tuple,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Scope* {
  // 0. Stamp the arguments with what they mean where they are written, before anything below copies them.
  for (auto const *arg : type_part.GnArgGroup->GetTypeArgs()) {
    if (arg->IsTypeArg()) { type_resolution::StampTypeParts(*arg->TypeVal, *sm->CurrentScope); }
  }
  for (auto const *arg : type_part.GnArgGroup->GetCompArgs()) {
    if (arg->IsCompArg()) { type_resolution::StampCompParts(*arg->CompVal, *sm->CurrentScope); }
  }

  // 1. Clone the template's scope. A class is the one construct whose instantiation gets a fresh scope rather than a
  // copy: its name is the instantiated type, not the template's, so only the symbols are carried over.
  const auto old_cls_scope = old_cls_sym->LinkedScope ? old_cls_sym->LinkedScope : old_cls_sym->ScopeDefinedIn;
  const auto name_clone = AstCloneShared(&type_part);

  // The clone names the instantiation; it is not written where the instantiation is used, so it answers no access
  // check there - a private "Box[S32]" reached inside std's "Vec[Box[S32]]" body is not std naming "Box".
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
    for (auto &arg : group->Args) {
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
  auto [new_cls_scope, new_cls_scope_ptr] = MakeUniqueAndRaw<Scope>(
    ScopeTypeIdentifierName(name_clone),
    old_cls_scope->Parent, old_cls_scope->AstNode);
  GiveScopeOwnSymbols(*new_cls_scope_ptr, *old_cls_scope, false);
  // The template's own non-generic scope: the class itself, or for an alias (a "use" of a generic class, linked to
  // its open instance "Atom[T=T]") the class the alias names.
  new_cls_scope_ptr->NonGnScope = old_cls_scope->NonGnScope;

  // Create a new class symbol, based on the new class scope, and copy over important information. The instantiation is
  // copyable, and a zero type, exactly when the template it substitutes is.
  const auto new_cls_sym = MakeShared<TypeSymbol>(
    name_clone, AstAs<ClassPrototypeAst>(new_cls_scope->AstNode), new_cls_scope.get(), sm->CurrentScope,
    old_cls_sym->Kind, old_cls_sym->IsDirectlyCopyable, old_cls_sym->Visibility);
  new_cls_sym->DerivesFromSymbol = old_cls_sym;

  // Filed under its identity before anything below analyses a type naming it. Its own members can ("Self", or a class
  // whose methods take one of itself), and a lookup from there must find this instantiation rather than mint another.
  new_cls_sym->InstanceOf = old_cls_sym.get();
  new_cls_sym->Id = args_id;
  if (auto *const tmpl = scopes::HeadOf(args_id).Symbol(); tmpl != nullptr) {
    tmpl->Instances[scopes::BareOf(args_id)] = new_cls_sym.get();
  }

  new_cls_sym->IsConcrete = scopes::IsConcreteTypeId(args_id);

  new_cls_scope_ptr->LinkedTypeSymbol = new_cls_sym;

  // An instantiation of a generic alias is the same alias under substituted arguments, so it gets its own
  // description rather than a clone of the syntax that produced it - only the resolved target actually differs.
  auto new_alias = Shared<AliasInfo>(nullptr);
  if (old_cls_sym->Alias != nullptr) {
    new_alias = MakeShared<AliasInfo>(*old_cls_sym->Alias);

    // A tuple's arguments are deliberately left positional. Workaround:
    if (is_tuple) {
      // Through "WithGns", which drops the written identity: a clone of the alias's target still carries the one naming the
      // tuple template's own instance ("Tup[Items=Items]"), which a lookup would follow instead of these arguments.
      new_alias->Resolved = old_cls_sym->Alias->Resolved->WithGns(AstClone(name_clone->GnArgGroup));
    }
    else {
      new_alias->Resolved = aliases::InstanceTargetOf(*old_cls_sym, args_id, *sm->CurrentScope);
    }
    new_alias->Resolved->Stage7_AnalyseSemantics(sm, meta);
    // TODO: Remove generic parameters that have been given arguments (not always all generic args).
    //  Move the argument filter out of the recursive alias searcher and reuse it here.
  }

  // 2. Attach the instantiation to the scope tree. An aliased type is attached where the alias was written, so that the
  // alias and the type it maps to are reachable from each other; anything else sits beside its own template.
  if (new_alias != nullptr) {
    new_alias->DeclaredIn()->AddTypeSymbol(new_cls_sym);
    new_alias->TrackingScope->AddTypeSymbol(new_cls_sym);
    new_alias->TrackingScope->Children.EmplaceBack(std::move(new_cls_scope));
    new_cls_sym->Alias = std::move(new_alias);
  }
  else {
    new_cls_scope_ptr->Parent->AddTypeSymbol(new_cls_sym);
    new_cls_scope_ptr->Parent->Children.EmplaceBack(std::move(new_cls_scope));
  }

  if (meta->CurrentStage >= meta::CompilerStage::kAttachSupScopes) {
    sm->AttachSpecificSuperScopes(*new_cls_scope_ptr, meta);
  }

  // Register the instantiation's own ast against the template. Its parameter list is emptied, which is what marks it as
  // an instantiation rather than the template it was cloned from.
  auto new_ast = AstClone(AstAs<ClassPrototypeAst>(old_cls_scope->AstNode));
  new_ast->SetAstScope(new_cls_scope_ptr);
  new_ast->GnParamGroup->Params.Clear();
  const auto new_ast_ptr = new_ast.get();
  old_cls_sym->Type->AddGnSubstitution({.InstanceScope = new_cls_scope_ptr, .Proto = std::move(new_ast)});

  // No more work for tuples. This is needed to prevent recursive checks on the generics (variadics would become tuples,
  // infinitely).
  if (is_tuple) {
    return new_cls_scope_ptr;
  }

  // 3. Bind the generic parameters. A class resolves its arguments where the instantiation was written, so the outer
  // scope manager is the one that reads them.
  // Bound from the arguments as written, which were analysed where they are written: a copy would be read again from
  // here, and "Single[Arr[T]]" written inside "Single" re-keyed through this instantiation's own "T" nests forever.
  // An instance's identity holds its arguments' ("InstanceIdOfArgs"); an alias's or a variant's is keyed another way.
  auto const &head = scopes::HeadOf(args_id);
  RegisterGnSymbols(
    head.Kind == TypeKey::Tag::Inst
    ? head.Args
    : sm->CurrentScope->ArgsIdOf(type_part.GnArgGroup->GetAllArgs(), old_cls_sym->GnParams()),
    new_cls_scope_ptr, sm, meta);

  // An alias's arguments bind the alias's own parameters; the class it names has its own, bound by the alias's target
  // ("MyVec[ZZ=Bool]" is "Vec[T=Bool, A=GlobalAlloc]"), which is what its members' types name.
  // A tuple's arguments stay positional, and name no parameter to bind.
  if (auto const &alias = new_cls_sym->Alias; alias != nullptr and not is_tuple and alias->Resolved != nullptr) {
    auto const &target_args = alias->Resolved->LastTypePart()->GnArgGroup->Args;
    if (genex::all_of(target_args, [](auto const &arg) { return arg->KeywordName() != nullptr; })) {
      const auto target_arg_ptrs = target_args | genex::views::transform([](auto const &a) { return a.get(); }) |
        genex::to<Vec>();
      RegisterGnSymbols(sm->CurrentScope->ArgsIdOf(target_arg_ptrs, nullptr), new_cls_scope_ptr, sm, meta);
    }
  }
  new_cls_scope_ptr->AddTypeSymbol(ScopeManager::MakeSelfTypeSymbol(new_cls_scope_ptr, new_cls_scope_ptr, 0));

  // 4. Read what the clone inherited through the bindings: the attribute symbols, and the attribute asts they came
  // from.
  // Todo: an aliased instantiation arguably wants its substituted types read through the scope the alias maps onto,
  //  rather than its own. The branch that did that was dead (it tested a unique_ptr that had already been moved into
  //  the symbol), so it is left out here rather than silently switched on.
  auto tm = ScopeManager(sm->GlobalScope, new_cls_scope_ptr);
  // "Self" is the instantiation by its identity: its spelled name reads its arguments as written, not as bound here.
  const auto self_type = new_cls_scope_ptr->TypeAstOf(args_id);
  ReadVarSymbolTypes(
    *new_cls_scope_ptr, &tm, meta, self_type != nullptr ? self_type.get() : new_cls_sym->FqName().get(),
    new_cls_scope_ptr->NonGnScope);

  for (auto *attr : new_ast_ptr->Impl->Members
       | genex::views::ptr
       | genex::views::cast_dynamic<ClassAttributeAst*>()) {
    //
    if (meta->CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
      // An attribute holds a value, so is never abstract, even
      // when the instantiation is reached from somewhere that
      // allows naming one ("B[C[A]]" instantiates "C[A]" while
      // "B"'s arguments are being analysed).
      const auto _meta_guard = meta::MetaGuard(meta);
      meta->AllowAbstractType = false;
      attr->Stage7_AnalyseSemantics(&tm, meta);
    }
  }

  return new_cls_scope_ptr;
}

auto spp::analyse::utils::monomorphization::CreateGnFnScope(
  Scope const &old_fun_scope,
  const TypeId args_id,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Unique<Scope> {
  // 1. Clone the template's scope. The whole subtree is cloned, because a function's parameters and locals live below
  // the scope handed in here (which is the mock "sup" scope stage 1 lowers the function into), and all of them carry
  // types written in terms of the template's parameters.
  auto [new_fun_scope, new_fun_scope_ptr] = MakeUniqueAndRaw<Scope>(old_fun_scope);
  GiveScopeOwnSymbols(*new_fun_scope_ptr, old_fun_scope, true);

  // 2. Bind the generic parameters, against the instantiation itself.
  auto tm = ScopeManager(sm->GlobalScope, new_fun_scope_ptr);
  RegisterGnSymbols(args_id, new_fun_scope_ptr, &tm, meta);

  // 3. Read what the clone inherited through the bindings, over the whole subtree. "Self" means one thing across the
  // instantiation, so it is looked up once, from the root.
  const auto self_type = meta->CurrentStage >= meta::CompilerStage::kGenTopLvlAliases
    ? new_fun_scope_ptr->FindEnclosingSelfType(*meta)
    : nullptr;
  const auto substitute_subtree = [&](auto const &self, Scope *scope, const bool is_root) -> void {
    // The bindings were registered on the root, the mock "sup" block stage 1 lowers the function into, but the function
    // scope one level down declares the function's own parameters again, as unbound symbols of the very same names. A
    // lookup from the body reaches those first and never sees the binding, leaving "SizedIntegerUnsigned[w]" symbolic
    // in an instantiation that knows exactly what "w" is. Drop the shadowing copies so the names resolve to what this
    // instantiation bound them to; the subtree is private to it, and the declaration itself still lives on the
    // prototype. Parameters inherited from an enclosing "sup" block declare no copies to drop.
    // The copy dropped is the parameter the root's binding binds, which the root could not see to record: it is
    // recorded now, so the binding is found by the parameter's identity, as a class's or a block's is.
    const auto rebind = [&](auto const &removed, auto &&root_rem, auto &&root_add) {
      if (removed == nullptr or removed->OwnParamId == 0) { return; }
      if (auto binding = root_rem(); binding != nullptr) {
        if (binding->BindsParamId == 0) { binding->BindsParamId = removed->OwnParamId; }
        root_add(std::move(binding));
      }
    };
    if (not is_root and args_id != nullptr) {
      for (auto const &arg : scopes::ArgsOf(args_id)) {
        if (arg.TypeVal != nullptr) {
          const auto name = BindingTypeName(arg);
          rebind(scope->RemTypeSymbol(name.get()),
                 [&] { return new_fun_scope_ptr->RemTypeSymbol(name.get()); },
                 [&](auto &&b) { new_fun_scope_ptr->AddTypeSymbol(b); });
        }
        else {
          const auto name = BindingCompName(arg);
          rebind(scope->RemVarSymbol(name.get()),
                 [&] { return new_fun_scope_ptr->RemVarSymbol(name.get()); },
                 [&](auto &&b) { new_fun_scope_ptr->AddVarSymbol(b); });
        }
      }
    }

    auto stm = ScopeManager(sm->GlobalScope, scope);
    ReadVarSymbolTypes(*scope, &stm, meta, self_type.get());
    for (auto const &child : scope->Children) { self(self, child.get(), false); }
  };
  substitute_subtree(substitute_subtree, new_fun_scope_ptr, true);

  return std::move(new_fun_scope);
}

auto spp::analyse::utils::monomorphization::CreateGnSupScope(
  Scope &old_sup_scope,
  Scope &new_cls_scope,
  const TypeId args_id,
  ScopeManager const *sm,
  meta::CompilerMetaData *meta)
  -> Tup<Scope*, Scope*> {
  // 1. Clone the template's scope. The subtree is cloned with it (see "Scope"'s copy constructor), so each member
  // function's block has a copy under the instantiation, resolving through its bindings; but only the block's own
  // symbols are made its own. The members are functions, each instantiated in its own right by
  // "CreateGnFnScope" when it is called.
  auto new_sup_scope = MakeUnique<Scope>(old_sup_scope);
  auto new_sup_scope_ptr = new_sup_scope.get();
  GiveScopeOwnSymbols(*new_sup_scope_ptr, old_sup_scope, false);

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
  for (auto i = 0uz; i < old_sup_scope.Children.Len() and i < new_sup_scope_ptr->Children.Len(); ++i) {
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
      ScopeManager::NormalSupBlocks[mock].EmplaceBack(child.get());
    }
  }

  // 2. Attach the instantiation to the scope tree, beside the template it was cloned from.
  old_sup_scope.Parent->Children.EmplaceBack(std::move(new_sup_scope));

  // 3. Bind the generic parameters, against the instantiation itself.
  auto tm = ScopeManager(sm->GlobalScope, new_sup_scope_ptr);
  RegisterGnSymbols(args_id, new_sup_scope_ptr, &tm, meta);
  new_sup_scope_ptr->AddTypeSymbol(ScopeManager::MakeSelfTypeSymbol(&new_cls_scope, new_sup_scope_ptr, 0));

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
  const auto class_args = new_cls_scope.LinkedTypeSymbol != nullptr
    ? scopes::InstanceBindings(TypeRef::ForKindCheck(new_cls_scope))
    : scopes::GenericSubst();
  ReadVarSymbolTypes(*new_sup_scope_ptr, &tm, meta, nullptr, nullptr, class_args);

  // Create the scope for the new super class type. This will handle recursive sup-scope creation.
  auto super_cls_scope = static_cast<Scope*>(nullptr);
  if (const auto ext_ast = AstAs<SupPrototypeExtensionAst>(old_sup_scope.AstNode); ext_ast != nullptr) {
    const auto new_fq_super_type = type_resolution::ReadType(*ext_ast->SuperCls, ExprSubst::In(*new_sup_scope_ptr));
    AnalyseSubstitutedType(*new_fq_super_type, &tm, meta, true, true);

    // Resolved where it was analysed: a super class naming a generic the instantiation carries is registered with
    // that generic, which the class being attached to has no path to.
    const auto super_cls_sym = TypeRef::Of(*new_fq_super_type, *new_sup_scope_ptr).Symbol;
    super_cls_scope = super_cls_sym != nullptr ? super_cls_sym->LinkedScope : nullptr;
  }

  return {new_sup_scope_ptr, super_cls_scope};
}

auto spp::analyse::utils::monomorphization::FindOrMakeGnSubstitutedPrototype(
  FunctionPrototypeAst *fn_proto,
  Scope const *fn_scope,
  const TypeId args_id,
  Shared<TypeAst> const &variadic_pack_type,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Tup<FunctionPrototypeAst*, Scope const*> {
  //
  using errors::SppSecondClassBorrowViolationError;
  using monomorphization::CreateGnFnScope;
  using type_predicates::IsTypeBorrowed;

  // Filed under what its arguments mean at the call, less any restating its own parameter, and with the variadic
  // pack's tuple; nothing left is the template itself. Reused if one exists for that identity already.
  const auto id = CallArgsId(*fn_proto, CalleeScope(*fn_proto, *fn_scope), args_id, variadic_pack_type,
                             *sm->CurrentScope);
  if (id != nullptr) {
    if (auto const *const existing = fn_proto->FindGnSubstitution(id); existing != nullptr) {
      return {existing->Proto.get(), existing->WalkScope()};
    }

    // Refused if it is one more step of a recursion that never ends.
    auto const *const requested_by = FunctionPrototypeAst::AnalysingGnSubstitution();
    const auto depth = FnInstantiationDepthOf(*fn_proto, requested_by);
    EnforceFnInstantiationBounded(*fn_proto, depth, id, *sm);

    auto new_fn_proto = AstClone(fn_proto);
    new_fn_proto->SetNonGnImpl(fn_proto);
    new_fn_proto->DetachLlvmFnSlot();

    // Create the new function scope for the generic implementation.
    auto new_fn_scope_owned = CreateGnFnScope(*fn_scope, id, sm, meta);
    const auto new_fn_scope = new_fn_scope_owned.get();
    auto tm = ScopeManager(sm->GlobalScope, new_fn_scope);

    // What "Self" is in the instantiation, read once for everything below that retypes by it: what it pins "Self" to
    // (a call's receiver), and what "Self" then means in its scope - the pin, else the owner.
    const auto pin = scopes::FindArgOf(id, 0);
    const auto pinned_self = pin.has_value() and pin->TypeVal != nullptr and pin->TypeVal != scopes::ParamTypeId(0)
      ? sm->CurrentScope->TypeAstOf(pin->TypeVal)
      : nullptr;
    const auto instance_self = new_fn_scope->FindEnclosingSelfType(*meta);

    // Drop only the parameters this substitution actually bound.
    new_fn_proto->GnParamGroup->Params |= genex::actions::remove_if([&](auto const &param) {
      return genex::any_of(scopes::ArgsOf(id), [&](auto const &arg) {
        return scopes::ArgNameOf(arg) == param->Name->ToString();
      });
    });

    // Substitute and analyse the function parameters and return type. Each parameter's type is also kept by identity:
    // the template's, substituted once with this instantiation's bindings ("FunctionParameterAst::InstanceTypeId"),
    // which is what the instantiation's signature is read as: its own scope would read a binding naming its own
    // parameter again ("f[(T, T)]" called inside "f[T]").
    const auto instance_bindings = scopes::BindingsOfArgs(id);
    auto const &template_scope = CalleeScope(*fn_proto, *fn_scope);
    const auto declared_id_of = [&](FunctionParameterAst const &p) -> TypeId {
      const auto name = p.ExtractName();
      for (auto const *tp : fn_proto->FnParamGroup->GetNonSelfParams()) {
        if (tp->ExtractName()->Val == name->Val) { return template_scope.TypeIdOf(*tp->Type); }
      }
      return nullptr;
    };
    for (auto *p : new_fn_proto->FnParamGroup->GetNonSelfParams()) {
      const auto declared = declared_id_of(*p);
      p->InstanceTypeId = declared != nullptr ? scopes::SubstituteTypeId(declared, instance_bindings) : nullptr;
      p->Type = type_resolution::ReadType(*p->Type, ExprSubst::In(*new_fn_scope));
      p->Type->Stage7_AnalyseSemantics(&tm, meta);

      // The default is checked against the substituted type, so it is rebuilt from what was written, in the same
      // terms. The template's copy carries the template's analysis: "Wrap[T]::new()" would still return "Wrap[T=T]".
      if (auto *const opt = p->To<FunctionParameterOptionalAst>(); opt != nullptr and opt->Source.OriginalDefaultVal !=
        nullptr) {
        opt->DefaultVal = AstClone(opt->Source.OriginalDefaultVal->ReadExpr(ExprSubst::In(*new_fn_scope)));
      }
    }

    ReattachCallableConstraints(*new_fn_proto, *fn_proto, new_fn_scope, tm, meta);
    RetypeInheritedSymbols(*new_fn_proto, new_fn_scope, pinned_self, variadic_pack_type, tm, meta);

    if (const auto declared = template_scope.TypeIdOf(*fn_proto->ReturnType); declared != nullptr) {
      new_fn_proto->InstanceReturnTypeId = scopes::SubstituteTypeId(declared, instance_bindings);
    }
    new_fn_proto->ReturnType = type_resolution::ReadType(*new_fn_proto->ReturnType, ExprSubst::In(*new_fn_scope));
    new_fn_proto->ReturnType->Stage7_AnalyseSemantics(&tm, meta);

    // Check the new return type isn't a borrow type.
    RaiseIf<SppSecondClassBorrowViolationError>(
      IsTypeBorrowed(*new_fn_proto->ReturnType, tm),
      {sm->CurrentScope},
      ERR_ARGS(*new_fn_proto->ReturnType, *new_fn_proto->ReturnType, "substituted function return type"));

    const auto is_concrete = ComputeIsConcrete(
      id, *new_fn_proto, new_fn_scope, instance_self.get(), tm, meta);

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

auto spp::analyse::utils::monomorphization::InstantiateOverload(
  FunctionPrototypeAst *fn_proto,
  Scope const *fn_scope,
  const TypeId args_id,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> FunctionPrototypeAst* {
  // The arguments arrive already named - they are read off a "sup" block that has bound them - so there is nothing
  // here for inference to do, and the substitution itself is the whole of what a call site would reach.
  auto *const instance = std::get<0>(FindOrMakeGnSubstitutedPrototype(
    fn_proto, fn_scope, args_id, nullptr, sm, meta));
  instance->RequireGnSubstitution();
  return instance;
}

auto spp::analyse::utils::monomorphization::FindInstantiatedOverload(
  FunctionPrototypeAst *fn_proto,
  const TypeId args_id,
  ScopeManager const *sm)
  -> FunctionPrototypeAst* {
  // Nothing to substitute means the template is the only prototype there is, exactly as
  // "FindOrMakeGnSubstitutedPrototype" decides it.
  const auto id = CallArgsId(
    *fn_proto, CalleeScope(*fn_proto, *sm->CurrentScope),
    args_id, nullptr, *sm->CurrentScope);
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
  if (fn_template == nullptr) { return; }
  if (not _Queued.insert(fn_template).second) { return; }
  _Pending.EmplaceBack(fn_template);
}

auto spp::analyse::utils::monomorphization::PopInstantiation()
  -> FunctionPrototypeAst* {
  // Nothing left, so release what the drain accumulated. Doing
  // it here rather than leaving a spent cursor behind is what
  // lets a later compilation in the same process start from an
  // empty record.
  if (_Cursor >= _Pending.Len()) {
    ClearInstantiations();
    return nullptr;
  }

  // Membership is dropped as the entry is handed out, not when
  // it was recorded, so a template instantiated again while it
  // is being drained is recorded afresh rather than silently
  // skipped.
  const auto fn_template = _Pending[_Cursor++];
  _Queued.erase(fn_template);
  return fn_template;
}

auto spp::analyse::utils::monomorphization::ClearInstantiations() -> void {
  _Pending.Clear();
  _Queued.clear();
  _Cursor = 0uz;
}

auto spp::analyse::utils::monomorphization::IsInTemplate(
  Scope const &scope)
  -> bool {
  for (auto const *s = &scope; s != nullptr; s = s->Parent) {
    if (genex::any_of(s->GetAllTypeSymbols(true), [](auto const *sym) { return sym->Kind == TypeKind::GnTypeParam; })) {
      return true;
    }
    if (genex::any_of(s->GetAllVarSymbols(true), [](auto const *sym) {
      return sym->Kind == VariableKind::GnCompParam;
    })) { return true; }
  }
  return false;
}
