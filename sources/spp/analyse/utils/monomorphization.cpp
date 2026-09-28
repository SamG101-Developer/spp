module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.monomorphization;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.packs;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.codegen.llvm_ctx;
import spp.utils.algorithms;
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
     * @param combined_generics The arguments this instantiation pins.
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param meta Associated metadata.
     */
    auto ReattachCallableConstraints(
      FunctionPrototypeAst const &new_fn_proto,
      FunctionPrototypeAst const &fn_proto,
      Scope *new_fn_scope,
      GenericArgumentGroupAst const &combined_generics,
      ScopeManager &tm,
      meta::CompilerMetaData *meta)
      -> void {
      for (auto *p : new_fn_proto.FnParamGroup->GetNonSelfParams()) {
        const auto declared = p->Source.OriginalType;
        if (declared == nullptr) { continue; }

        const auto gn_param = genex::find_if(
          fn_proto.GnParamGroup->Params, [&](auto const &g) { return *g->Name == *declared; });
        if (gn_param == fn_proto.GnParamGroup->Params.end()) { continue; }

        auto const &constraints = *gn_param;
        if (constraints->Constraints == nullptr) { continue; }

        for (auto const &c : constraints->Constraints->Constraints) {
          const auto target = type_resolution::ThroughAlias(*c, *new_fn_scope);
          if (not type_predicates::IsTypeFunc(TypeRef::OfHead(*target, *new_fn_scope), *new_fn_scope)) { continue; }
          const auto sym = new_fn_scope->Children[0]->GetVarSymbol(p->ExtractName().get(), true);
          if (sym == nullptr) { break; }

          // Substituted the same way the parameter's own type is: the constraint is written in the template's terms
          // ("FunMov[(T,), U]"), and what the call needs is this instantiation's argument and return types.
          auto callable = c->SubstituteGenerics(combined_generics.GetAllArgs());
          callable->Stage7_AnalyseSemantics(&tm, meta);
          sym->CallableAsType = type_resolution::ThroughAlias(*callable, *new_fn_scope);
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
     * @param combined_generics The arguments this instantiation pins.
     * @param variadic_pack_type The tuple the call collapsed its trailing arguments into, or @c nullptr .
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param meta Associated metadata.
     */
    auto RetypeInheritedSymbols(
      FunctionPrototypeAst &new_fn_proto,
      Scope *new_fn_scope,
      GenericArgumentGroupAst const &combined_generics,
      Shared<TypeAst> const &variadic_pack_type,
      ScopeManager &tm,
      meta::CompilerMetaData *meta)
      -> void {
      // "self" is typed as "Self", so only a substitution that pins "Self" to the receiver has anything to rewrite.
      if (const auto self_param = new_fn_proto.FnParamGroup->GetSelfParam(); self_param != nullptr) {
        auto substituted_self = self_param->Type->SubstituteGenerics(combined_generics.GetAllArgs());
        if (not substituted_self->IsSelfType()) {
          substituted_self->Stage7_AnalyseSemantics(&tm, meta);
          self_param->Type = substituted_self;
          const auto self_name = self_param->ExtractName();
          if (const auto self_sym = new_fn_scope->Children[0]->GetVarSymbol(self_name.get(), true);
            self_sym != nullptr) {
            self_sym->Type = substituted_self->WithConvention(AstClone(self_param->Conv));
          }
        }
      }
      new_fn_proto.VariadicPackType = AstClone(variadic_pack_type);

      // A variadic parameter declares one element but binds the whole tuple.
      if (variadic_pack_type != nullptr) {
        auto pack_type = AstClone(variadic_pack_type);
        pack_type->Stage7_AnalyseSemantics(&tm, meta);
        const auto pack_name = new_fn_proto.FnParamGroup->GetVariadicParams()->ExtractName();
        if (const auto pack_sym = new_fn_scope->Children[0]->GetVarSymbol(pack_name.get(), true);
          pack_sym != nullptr) {
          pack_sym->Type = std::move(pack_type);
        }
      }
    }

    /**
     * Whether an instantiation is concrete: every argument it pins names a real type or value, and every type in its
     * own signature resolves to one. A non-concrete instantiation is still a template as far as anything downstream is
     * concerned, so codegen has nothing to emit for it.
     * @param combined_generics The arguments this instantiation pins.
     * @param new_fn_proto The instantiated prototype whose signature is checked.
     * @param new_fn_scope The instantiation's scope, which the signature's types are resolved in.
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param sm The scope manager at the call site, which the arguments are resolved in.
     * @param meta Associated metadata.
     * @return If the instantiation is fully concrete.
     */
    auto ComputeIsConcrete(
      GenericArgumentGroupAst const &combined_generics,
      FunctionPrototypeAst const &new_fn_proto,
      Scope const *new_fn_scope,
      ScopeManager &tm,
      ScopeManager const *sm,
      meta::CompilerMetaData *meta)
      -> bool {
      const auto type_is_concrete = [&](TypeAst const &type) {
        const auto resolved = self_type::SubstituteSelfTypeAndAnalyse(type, *new_fn_scope, tm, *meta);
        return type_predicates::IsTypeFullyConcrete(*resolved, *new_fn_scope);
      };

      // The arguments are asked the same way a class instantiation asks them; a function goes on to check the
      // signature it ended up with, which a class has no equivalent of.
      return type_predicates::AreGenericArgsConcrete(combined_generics.Args, *sm->CurrentScope)
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
     * Rewrite each generic argument to what it actually names at the call site, then drop the ones that only restate
     * their own parameter. A type argument naming a bound generic becomes the type it is bound to; a comp argument
     * naming a bound comp generic becomes the value read back off its symbol. An unbound parameter names nothing yet
     * and is left alone, which is what keeps a template's signature written in its own terms.
     * @param combined_generics The argument group to normalise, rewritten in place.
     * @param callee_scope The scope the callee's generic parameters are looked up from.
     * @param sm The scope manager, positioned at the call site.
     */
    auto NormaliseGenericArgs(
      GenericArgumentGroupAst &combined_generics,
      Scope const &callee_scope,
      ScopeManager const *sm)
      -> void {
      // Bound generics are written as what they are bound to, as a class instantiation's arguments are.
      monomorphization::CanonicaliseGenericArgs(combined_generics, *sm->CurrentScope);

      // Drop the arguments that name the very parameter they bind - a call made inside the generic context declaring
      // it, as when one method of a generic "sup" block calls another. What is left is what this instantiation actually
      // pins, and if that is nothing then there is no instantiation to make. Only the same declaration counts, which a
      // parameter's "ParamId" identifies (a binding records the one it binds): another context's
      // parameter of the same name is another type, and gets an instantiation of its own.
      const auto names_own_param = [&](auto const &a) {
        if (a->Name != nullptr and a->TypeVal != nullptr) {
          const auto param_sym = callee_scope.GetTypeSymbol(a->Name.get());
          const auto val_sym = sm->CurrentScope->GetTypeSymbol(a->TypeVal.get());
          return param_sym != nullptr and val_sym != nullptr and param_sym->ParamId != 0
            and (val_sym->ParamId == param_sym->ParamId or val_sym->BindsParamId == param_sym->ParamId);
        }
        if (a->Name != nullptr and a->CompVal != nullptr) {
          const auto val_ident = a->CompVal->template To<IdentifierAst>();
          if (val_ident == nullptr) { return false; }
          const auto param_sym = callee_scope.GetVarSymbol(IdentifierAst::FromType(*a->Name).get());
          return param_sym != nullptr and param_sym == sm->CurrentScope->GetVarSymbol(val_ident);
        }
        return false;
      };
      combined_generics.Args |= genex::actions::remove_if(names_own_param);
    }


    /**
     * The final type part of a name, as an owning pointer. The part accessors hand back borrowed pointers, because
     * almost every caller only reads through them, but a symbol keeps its name for as long as it lives; the part is
     * held by the name it was read out of, so ownership is recovered from the node rather than by cloning it, which
     * would leave the symbol naming a node that compares equal to the written type without being it.
     */
    auto NameLastTypePart(
      TypeAst const &name)
      -> Shared<TypeIdentifierAst> {
      auto *const part = const_cast<TypeAst&>(name).LastTypePart();
      return static_shared_cast<TypeIdentifierAst>(part->shared_from_this());
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
    auto GiveScopeOwnSyms(
      Scope &dst,
      Scope const &src,
      const bool recurse)
      -> void {
      dst.InternalTable.DeepCopyFrom(src.InternalTable);
      if (not recurse) { return; }
      for (auto i = 0uz; i < dst.Children.Len() and i < src.Children.Len(); ++i) {
        GiveScopeOwnSyms(*dst.Children[i].get(), *src.Children[i].get(), true);
      }
    }

    /**
     * Record an instantiation's own name in terms that mean the same wherever it is read. A type argument naming a
     * generic bound to a closed type is stamped with that type, and keeps its spelling: the name is only printed, while
     * every lookup (and "SubstituteGenerics", which leaves a name stamped with anything but a parameter alone) follows
     * the stamp. A generic bound to another open type stays as written: an open name built from the minting scope's
     * bindings would grow when read again ("Single[Arr[T]]" inside "Single"). A comp argument whose value is closed is
     * written as what it folds to.
     * @param args The instantiation's own arguments, a copy private to its name.
     * @param scope The scope the instantiation is made from, whose bindings the arguments are read through.
     */
    auto StampClosedBindings(
      GenericArgumentGroupAst &args,
      Scope const &scope)
      -> void {
      for (auto const &arg : args.Args) {
        if (arg->Name == nullptr or arg->TypeVal == nullptr) { continue; }
        const auto val_sym = scope.GetTypeSymbol(arg->TypeVal.get());
        if (val_sym == nullptr or not val_sym->IsTypeGeneric() or val_sym->Type == nullptr) { continue; }
        if (val_sym->LinkedScope == nullptr or val_sym->LinkedScope->TySym == nullptr) { continue; }
        if (not type_predicates::IsTypeFullyConcrete(*val_sym->FqName(), scope)) { continue; }
        auto *const bound = val_sym->LinkedScope->TySym.get();
        arg->TypeVal->SetStamp(bound);
        arg->TypeVal->LastTypePart()->SetStamp(bound);
      }
      for (auto &arg : args.Args) {
        if (arg->Name == nullptr or arg->CompVal == nullptr) { continue; }
        if (auto folded = comp_generics::FoldCompExpr(*arg->CompVal, scope); folded != nullptr) {
          arg->CompVal = std::move(folded);
        }
      }
    }

    /**
     * Create the symbol that binds one generic parameter to the argument given for it: a type symbol naming the bound
     * type, or a variable symbol carrying the bound comp-time value.
     * @param generic The generic argument being bound.
     * @param sm The scope manager whose current scope the argument is resolved against.
     * @param meta The compiler meta data.
     * @param tm An alternative scope manager to infer a comp-time argument's type through.
     * @return The symbol for the binding.
     */
    auto CreateGenericSym(
      GenericArgumentAst const &generic,
      ScopeManager &sm,
      meta::CompilerMetaData *meta,
      ScopeManager *tm = nullptr)
      -> Shared<Symbol> {
      //
      using errors::SppInternalCompilerError;

      // An argument is a named type or a named comp value; anything else is a bug.
      if (generic.Name == nullptr or (generic.TypeVal == nullptr and generic.CompVal == nullptr)) {
        Raise<SppInternalCompilerError>(
          {sm.CurrentScope},
          ERR_ARGS(generic, "Unknown generic argument ast type"));
      }

      // Handle the generic type argument => creates a type symbol.
      if (generic.TypeVal != nullptr) {
        // "Self" should not be looked up and changed.
        if (generic.TypeVal->IsSelfType()) {
          return MakeShared<TypeSymbol>(
            NameLastTypePart(*generic.Name), nullptr, nullptr, sm.CurrentScope, TypeKind::GenericArg);
        }

        auto true_val_sym = sm.CurrentScope->GetTypeSymbol(generic.TypeVal.get());

        // An alias is bound as its target, as "InstanceIdentityKey" keys it: the alias links whatever its target had
        // resolved to when it was last analysed, which for one analysed early is the target's template. A target that
        // only reaches its template yet (its instantiation is not made) keeps the alias, which still tells them apart.
        if (true_val_sym != nullptr and true_val_sym->Alias != nullptr and true_val_sym->Alias->Resolved != nullptr) {
          if (auto *const resolved = sm.CurrentScope->GetTypeSymbol(true_val_sym->Alias->Resolved.get());
            resolved != nullptr and (resolved->IsConcrete or resolved->InstanceOf != nullptr)) { true_val_sym = resolved; }
        }

        // Build the type symbol for the generic type argument.
        auto sym = MakeShared<TypeSymbol>(
          NameLastTypePart(*generic.Name), true_val_sym ? true_val_sym->Type : nullptr,
          true_val_sym ? true_val_sym->LinkedScope : nullptr, sm.CurrentScope, TypeKind::GenericArg,
          true_val_sym ? true_val_sym->IsDirectlyCopyable : false, asts::utils::Visibility::kPublic,
          AstClone(generic.TypeVal->GetConvention()));
        sym->GenericConstraints = true_val_sym
          ? true_val_sym->GenericConstraints
          : decltype(true_val_sym->GenericConstraints){};
        sym->IsDirectlyZeroType = true_val_sym
          ? true_val_sym->IsDirectlyZeroType
          : false;

        // Record what the parameter was bound to. When the value is another (unresolved) generic parameter there is no
        // linked scope to recover the binding from later, so the value type is the only record of it.
        sym->GenericVal = AstCloneShared(generic.TypeVal);
        return sym;
      }

      // Handle the generic comp argument => creates a variable symbol.
      auto sym = MakeShared<VariableSymbol>(
        IdentifierAst::FromType(*generic.Name),
        generic.CompVal->InferType(tm ? tm : &sm, meta),
        sm.CurrentScope,
        VariableKind::GenericCompArg, false, asts::utils::Visibility::kPublic);

      // A comp argument naming a bound comp generic binds what that is bound to where it is written, as a type
      // argument binds the type it names there ("SizedInteger[w]" in a "[cmp w: U32]" instance binds 32, not "w").
      // A closed value binds what it folds to ("n + 1_uz" with "n" bound to "1_uz" binds "2_uz").
      auto value = comp_generics::ResolveCompArg(*generic.CompVal, *sm.CurrentScope);
      sym->CompTimeValue = value != nullptr ? std::move(value) : AstClone(generic.CompVal);
      return sym;
    }

    /**
     * Bind the generic parameters of an instantiation: register a symbol per generic argument into the instantiation's
     * scope. Nothing is carried in from where the instantiation was named: an argument naming a generic there is stamped
     * with it, and read by identity ("Scope::Canon"), not by spelling.
     * @param generic_args The arguments the generic parameters are being bound to.
     * @param scope The instantiation's scope to register the symbols into.
     * @param sm The scope manager the arguments are resolved against. A class resolves them where the instantiation was
     * written, because that is where its argument types are named; a "sup" block or function resolves them against the
     * instantiation itself.
     * @param meta The compiler meta data.
     */
    auto RegisterGenericSyms(
      Vec<Unique<GenericArgumentAst>> const &generic_args,
      Scope *scope,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      // Convert the type arguments into symbols, all before any is registered, so each resolves as written rather
      // than against a binding made a moment earlier. The comp arguments wait until the type bindings are in: a comp
      // value is typed in this scope, and one naming a type parameter ("cmp p: Box[T]") needs "T" bound by then.
      auto type_syms = Vec<Shared<Symbol>>();
      auto comp_args = Vec<GenericArgumentAst const*>();
      for (auto const &g : generic_args) {
        if (g->Name != nullptr and g->CompVal != nullptr) { comp_args.EmplaceBack(g.get()); }
        else { type_syms.EmplaceBack(CreateGenericSym(*g, *sm, meta)); }
      }

      // Record which parameter a binding binds: the one this scope declares under its name (the symbol the binding
      // replaces), or else one inherited from an enclosing "sup" block, which was never declared here and is reached
      // through the parents ("inherited", only asked when needed).
      const auto bind_param = [](auto &binding, const std::uint64_t own_id, auto &&inherited) {
        if (own_id != 0) {
          binding.BindsParamId = own_id;
          return;
        }
        if (auto const *param = inherited(); param != nullptr) {
          binding.BindsParamId = param->ParamIdentity();
        }
      };

      // Register the bindings, replacing the template's own (unbound) parameter symbols of the same names. A type
      // parameter's constraints are written on the template, so they are carried over onto the binding.
      for (auto const &e : type_syms | spp::views::cast_shared<TypeSymbol>()) {
        const auto old = scope->RemTypeSymbol(e->Name.get());
        if (old) {
          e->GenericConstraints = AstCloneVecShared(old->GenericConstraints);
          e->IsVariadic = old->IsVariadic;
        }
        const auto own_id = old != nullptr and old->Kind == TypeKind::GenericParam ? old->ParamId : std::uint64_t{0};
        bind_param(*e, own_id, [&] {
          return scope->Parent != nullptr ? scope->Parent->GetTypeSymbol(e->Name.get()) : nullptr;
        });
        scope->AddTypeSymbol(e);
      }
      auto comp_syms = Vec<Shared<Symbol>>();
      for (auto const *g : comp_args) { comp_syms.EmplaceBack(CreateGenericSym(*g, *sm, meta)); }
      for (auto const &e : comp_syms | spp::views::cast_shared<VariableSymbol>()) {
        const auto old = scope->RemVarSymbol(e->Name.get());
        if (old) { e->IsVariadic = old->IsVariadic; }
        const auto own_id = old != nullptr and old->Kind == VariableKind::GenericCompParam
          ? old->ParamId
          : std::uint64_t{0};
        bind_param(*e, own_id, [&] {
          return scope->Parent != nullptr ? scope->Parent->GetVarSymbol(e->Name.get()) : nullptr;
        });
        scope->AddVarSymbol(e);
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
     * Substitute the generic bindings into the types the instantiation's variable symbols carry. Nothing else
     * re-derives them: a symbol's type is what @c InferType hands back verbatim, so a "that: SizedIntegerUnsigned[w]"
     * left holding the template's own "w" makes the instantiated body infer "w" from "w" and resolve back against the
     * template instead of the instantiation.
     * @param scope The instantiation's scope whose own variable symbols are being substituted.
     * @param generic_args The arguments the generic parameters were bound to.
     * @param tm The scope manager to analyse the substituted types through.
     * @param meta The compiler meta data.
     */
    auto SubstituteVarSymTypes(
      Scope const &scope,
      Vec<GenericArgumentAst*> const &generic_args,
      ScopeManager *tm,
      meta::CompilerMetaData *meta,
      TypeAst const *self_type = nullptr)
      -> void {
      for (auto const &scoped_sym : scope.AllVarSymbols(true)) {
        if (scoped_sym->Type == nullptr) { continue; }

        // A "Self" is replaced by what it means in the instantiation
        // ("self_type") before the bindings go in: "self: Self" in
        // "sup [cmp w: U32] SizedInteger[w, false]" is
        // "SizedInteger[w, false]", then "SizedInteger[64_u32,
        // false]". The template resolves its own in stage 7, so a
        // copy taken earlier - a comp expression in a signature,
        // checked in stage 6 - still holds the bare "Self", which
        // means nothing outside it.
        if (self_type != nullptr and type_predicates::NamesSelfType(*scoped_sym->Type)) {
          scoped_sym->Type = self_type::SubstituteSelfTypeWith(*scoped_sym->Type, *self_type);
        }
        scoped_sym->Type = scoped_sym->Type->SubstituteGenerics(generic_args);
        if (meta->CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
          // Note: DO NOT inline "analysed_type", because the scoped_sym->Type
          // can change during the analysis that uses it, leaving the original
          // dereference pointing to garbage.
          const auto analysed_type = scoped_sym->Type;
          AnalyseSubstitutedType(*analysed_type, tm, meta, true, false);
        }
      }
    }

    /**
     * Register the "Self" type of an instantiation, which names the class the instantiation belongs to rather than the
     * template's own.
     * @param scope The instantiation's scope to register the symbol into.
     * @param cls_scope The scope of the class "Self" names.
     */
    auto AddSelfTypeSym(
      Scope &scope,
      Scope *cls_scope)
      -> void {
      scope.AddTypeSymbol(ScopeManager::MakeSelfTypeSymbol(cls_scope, &scope, 0));
    }

  }
}

auto spp::analyse::utils::monomorphization::InstantiateForScope(
  TypeSymbol &open_instance,
  Scope const &scope,
  Shared<Scope> const &global_scope,
  meta::CompilerMetaData *meta)
  -> TypeSymbol* {
  // Making it analyses a name, and anything that analysis asks for may ask for this again; that asks nothing further.
  static auto in_progress = std::vector<std::pair<TypeSymbol const*, Scope const*>>();
  const auto entry = std::make_pair(static_cast<TypeSymbol const*>(&open_instance), &scope);
  if (genex::find(in_progress, entry) != in_progress.end()) { return nullptr; }
  in_progress.push_back(entry);
  struct PopEntry {
    decltype(in_progress) &Entries;
    ~PopEntry() { Entries.pop_back(); }
  } const _pop{in_progress};

  // Its own qualified name, analysed here: the arguments are read through this scope's bindings, and the analysis files
  // the instantiation under that identity, as any name written here would. The stamps naming the open one are dropped,
  // or the lookup would lead straight back to it, and so is the copy's analysed mark.
  // Its arguments are substituted with this scope's generics first, nested ones included, so the instantiation's own
  // name says what it is wherever it is read, as a substituted one's does.
  const auto generics = GenericArgumentGroupAst(nullptr, scope.GetGenerics(), nullptr);
  const auto type = open_instance.FqName()->SubstituteGenerics(generics.GetAllArgs());
  type->SetStamp(nullptr);
  type->LastTypePart()->SetStamp(nullptr);
  type->ResetCache();
  auto tm = ScopeManager(global_scope, const_cast<Scope*>(&scope));
  try { AnalyseSubstitutedType(*type, &tm, meta, true, true); }
  catch (errors::SemanticError const &) { return nullptr; }
  return type->LastTypePart()->Stamp();
}

auto spp::analyse::utils::monomorphization::MonomorphiseToFixedPoint(
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Every instantiation registered during stages 1-9 is already waiting, because registering one is what records its
  // template. Draining a template analyses whatever it has accumulated since, and that analysis records whatever it
  // reaches in turn, so the loop ends exactly when nothing new was found.
  while (auto *fn_template = monomorphization::PopInstantiation()) {
    fn_template->AnalysePendingGenericSubstitutions(sm, meta);
  }
  sm->Reset();
}

auto spp::analyse::utils::monomorphization::CanonicaliseGenericArgs(
  GenericArgumentGroupAst &args,
  Scope const &scope)
  -> void {
  // A type argument naming a generic bound to a closed type is stamped with it, exactly as a class instantiation's own
  // name records one ("StampClosedBindings"), rather than rewritten to that type's qualified name: the two paths meant
  // the same thing, and a stamp says it without changing what the argument is spelled as. An unbound parameter, or one
  // bound to another parameter, has no class to name and keeps its node and stamp. This also folds the comp arguments.
  StampClosedBindings(args, scope);

  // Beyond the folding above: for a function instantiation, a comp argument naming a comp generic bound to another is
  // written as that, which a class instantiation's name has no need of.
  for (auto &arg : args.Args) {
    if (arg->Name == nullptr or arg->CompVal == nullptr) { continue; }
    if (auto value = comp_generics::ResolveCompArg(*arg->CompVal, scope); value != nullptr) {
      arg->CompVal = std::move(value);
    }
  }
}

auto spp::analyse::utils::monomorphization::CreateGenericClsScope(
  TypeIdentifierAst &type_part,
  Shared<TypeSymbol> const &old_cls_sym,
  const bool is_tuple,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Scope* {
  // 0. Stamp the arguments with what they mean where they are written, before anything below copies them.
  for (auto const *arg : type_part.GnArgGroup->GetTypeArgs()) {
    if (arg->TypeVal != nullptr) { type_resolution::StampWrittenParts(*arg->TypeVal, *sm->CurrentScope); }
  }
  for (auto const *arg : type_part.GnArgGroup->GetCompArgs()) {
    if (arg->CompVal != nullptr) { comp_generics::StampCompGenerics(*arg->CompVal, *sm->CurrentScope); }
  }

  // The identity the instantiation is filed under, read where it is written, like its arguments.
  const auto identity_key = sm->CurrentScope->InstanceIdentityKey(type_part.GnArgGroup->GetAllArgs());

  // 1. Clone the template's scope. A class is the one construct whose instantiation gets a fresh scope rather than a
  // copy: its name is the instantiated type, not the template's, so only the symbols are carried over.
  const auto old_cls_scope = old_cls_sym->LinkedScope ? old_cls_sym->LinkedScope : old_cls_sym->ScopeDefinedIn;
  const auto name_clone = AstCloneShared(&type_part);

  // The clone names the instantiation; it is not written where the instantiation is used, so it answers no access
  // check there - a private "Box[S32]" reached inside std's "Vec[Box[S32]]" body is not std naming "Box".
  name_clone->ClearSourceWritten();

  // Recorded in terms that mean the same wherever it is read ("StampClosedBindings"). The use site keeps its own node,
  // resolved on read. The clone's arguments may have changed, so it keeps no stamp.
  StampClosedBindings(*name_clone->GnArgGroup, *sm->CurrentScope);
  name_clone->SetStamp(nullptr);
  auto [new_cls_scope, new_cls_scope_ptr] = MakeUniqueAndRaw<Scope>(
    ScopeTypeIdentifierName(name_clone),
    old_cls_scope->Parent, old_cls_scope->AstNode);
  GiveScopeOwnSyms(*new_cls_scope_ptr, *old_cls_scope, false);
  new_cls_scope_ptr->NonGenericScope = old_cls_scope;

  // Create a new class symbol, based on the new class scope, and copy over important information. The instantiation is
  // copyable, and a zero type, exactly when the template it substitutes is.
  const auto new_cls_sym = MakeShared<TypeSymbol>(
    name_clone, AstAs<ClassPrototypeAst>(new_cls_scope->AstNode), new_cls_scope.get(), sm->CurrentScope,
    old_cls_sym->Kind, old_cls_sym->IsDirectlyCopyable, old_cls_sym->Visibility);
  new_cls_sym->DerivesFromSym = old_cls_sym;

  // Filed under its identity before anything below analyses a type naming it. Its own members can ("Self", or a class
  // whose methods take one of itself), and a lookup from there must find this instantiation rather than mint another.
  new_cls_sym->InstanceOf = old_cls_sym.get();
  new_cls_sym->IdentityKey = identity_key;
  old_cls_sym->Instances[identity_key] = new_cls_sym.get();

  new_cls_sym->IsConcrete = type_predicates::AreGenericArgsConcrete(
    type_part.GnArgGroup->Args, *sm->CurrentScope);

  new_cls_scope_ptr->TySym = new_cls_sym;

  // An instantiation of a generic alias is the same alias under substituted arguments, so it gets its own
  // description rather than a clone of the syntax that produced it - only the resolved target actually differs.
  auto new_alias = Shared<AliasInfo>(nullptr);
  if (old_cls_sym->Alias != nullptr) {
    new_alias = MakeShared<AliasInfo>(*old_cls_sym->Alias);

    // A tuple's arguments are deliberately left positional. Workaround:
    if (is_tuple) {
      // Through "WithGenerics", which drops the stamp: a clone of the alias's target still carries the one naming the
      // tuple template's own instance ("Tup[Items=Items]"), which a lookup would follow instead of these arguments.
      new_alias->Resolved = old_cls_sym->Alias->Resolved->WithGenerics(AstClone(name_clone->GnArgGroup));
    }
    else {
      new_alias->Resolved = old_cls_sym->Alias->Resolved->SubstituteGenerics(name_clone->GnArgGroup->GetAllArgs());
    }
    new_alias->Resolved->Stage7_AnalyseSemantics(sm, meta);
    // TODO: Remove generic parameters that have been given arguments (not always all generic args).
    //  Move the argument filter out of the recursive alias searcher and reuse it here.
  }

  // 2. Attach the instantiation to the scope tree. An aliased type is attached where the alias was written, so that the
  // alias and the type it maps to are reachable from each other; anything else sits beside its own template.
  if (new_alias != nullptr) {
    new_alias->DeclScope->AddTypeSymbol(new_cls_sym);
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
  old_cls_sym->Type->RegisterGenericSubstitution(new_cls_scope_ptr, std::move(new_ast));

  // No more work for tuples. This is needed to prevent recursive checks on the generics (variadics would become tuples,
  // infinitely).
  if (is_tuple) {
    return new_cls_scope_ptr;
  }

  // 3. Bind the generic parameters. A class resolves its arguments where the instantiation was written, so the outer
  // scope manager is the one that reads them.
  // Bound from the arguments as written, which were analysed where they are written: a copy would be read again from
  // here, and "Single[Arr[T]]" written inside "Single" re-keyed through this instantiation's own "T" nests forever.
  RegisterGenericSyms(type_part.GnArgGroup->Args, new_cls_scope_ptr, sm, meta);
  AddSelfTypeSym(*new_cls_scope_ptr, new_cls_scope_ptr);

  // 4. Substitute the bindings into what the clone inherited: the attribute symbols, and the attribute asts they came
  // from. The instantiation's own fully qualified name contributes as well, because an alias binds parameters that the
  // written type does not name.
  const auto fq_type = new_cls_sym->FqName();
  auto substitution_generics = fq_type->LastTypePart()->GnArgGroup->GetAllArgs();
  substitution_generics.AppendRange(type_part.GnArgGroup->GetAllArgs());

  // Todo: an aliased instantiation arguably wants its substituted types read through the scope the alias maps onto,
  //  rather than its own. The branch that did that was dead (it tested a unique_ptr that had already been moved into
  //  the symbol), so it is left out here rather than silently switched on.
  auto tm = ScopeManager(sm->GlobalScope, new_cls_scope_ptr);
  SubstituteVarSymTypes(*new_cls_scope_ptr, substitution_generics, &tm, meta);

  for (auto *attr : new_ast_ptr->Impl->Members
       | genex::views::ptr
       | genex::views::cast_dynamic<ClassAttributeAst*>()) {
    //
    attr->Type = attr->Type->SubstituteGenerics(substitution_generics);
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

auto spp::analyse::utils::monomorphization::CreateGenericFunScope(
  Scope const &old_fun_scope,
  GenericArgumentGroupAst const &generic_args,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Scope* {
  // 1. Clone the template's scope. The whole subtree is cloned, because a function's parameters and locals live below
  // the scope handed in here (which is the mock "sup" scope stage 1 lowers the function into), and all of them carry
  // types written in terms of the template's parameters.
  auto [new_fun_scope, new_fun_scope_ptr] = MakeUniqueAndRaw<Scope>(old_fun_scope);
  GiveScopeOwnSyms(*new_fun_scope_ptr, old_fun_scope, true);

  // 2. Register the instantiation against the template, which takes ownership of its scope. Only the slot is reserved
  // here: the caller fills in the substituted prototype once it has substituted the signature.
  const auto old_fn_proto = AstBody(old_fun_scope.AstNode)[0]->To<FunctionPrototypeAst>();
  old_fn_proto->RegisterGenericSubstitution(
    std::move(new_fun_scope), nullptr,
    MakeUnique<GenericArgumentGroupAst>(nullptr, AstCloneVec(generic_args.Args), nullptr));

  // 3. Bind the generic parameters, against the instantiation itself.
  auto tm = ScopeManager(sm->GlobalScope, new_fun_scope_ptr);
  RegisterGenericSyms(generic_args.Args, new_fun_scope_ptr, &tm, meta);

  // 4. Substitute the bindings into what the clone inherited, over the whole subtree. "Self" means one thing across
  // the instantiation, so it is looked up once, from the root.
  const auto substitution_generics = generic_args.GetAllArgs();
  const auto self_type = meta->CurrentStage >= meta::CompilerStage::kGenTopLvlAliases
    ? new_fun_scope_ptr->GetEnclosingSelfType(*meta)
    : nullptr;
  const auto substitute_subtree = [&](auto const &self, Scope *scope, const bool is_root) -> void {
    // The bindings were registered on the root, the mock "sup" block stage 1 lowers the function into, but the function
    // scope one level down declares the function's own parameters again, as unbound symbols of the very same names. A
    // lookup from the body reaches those first and never sees the binding, leaving "SizedIntegerUnsigned[w]" symbolic
    // in an instantiation that knows exactly what "w" is. Drop the shadowing copies so the names resolve to what this
    // instantiation bound them to; the subtree is private to it, and the declaration itself still lives on the
    // prototype. Parameters inherited from an enclosing "sup" block declare no copies to drop.
    if (not is_root) {
      for (auto const &g : generic_args.Args) {
        if (g->Name == nullptr) { continue; }
        if (g->TypeVal != nullptr) { scope->RemTypeSymbol(g->Name->ToUnchecked<TypeIdentifierAst>()); }
        else { scope->RemVarSymbol(IdentifierAst::FromType(*g->Name).get()); }
      }
    }

    auto stm = ScopeManager(sm->GlobalScope, scope);
    SubstituteVarSymTypes(*scope, substitution_generics, &stm, meta, self_type.get());
    for (auto const &child : scope->Children) { self(self, child.get(), false); }
  };
  substitute_subtree(substitute_subtree, new_fun_scope_ptr, true);

  return new_fun_scope_ptr;
}

auto spp::analyse::utils::monomorphization::CreateGenericSupScope(
  Scope &old_sup_scope,
  Scope &new_cls_scope,
  GenericArgumentGroupAst const &generic_args,
  ScopeManager const *sm,
  meta::CompilerMetaData *meta)
  -> Tup<Scope*, Scope*> {
  // 1. Clone the template's scope. The subtree is cloned with it (see "Scope"'s copy constructor), so each member
  // function's block has a copy under the instantiation, resolving through its bindings; but only the block's own
  // symbols are made its own. The members are functions, each instantiated in its own right by
  // "CreateGenericFunScope" when it is called.
  auto new_sup_scope = MakeUnique<Scope>(old_sup_scope);
  auto new_sup_scope_ptr = new_sup_scope.get();
  GiveScopeOwnSyms(*new_sup_scope_ptr, old_sup_scope, false);

  // A "cmp" on a generic "sup" block mangles to "<module>#<name>": "mangle_mod_name" drops every "<...>" scope, so
  // neither the block nor its generic arguments reach the name, and one written constant is one global however many
  // instantiations name it. The copy above therefore has to be undone for the llvm record specifically, or stage 10 -
  // which only ever walks the template - would give storage to a symbol that "Atom[Bool]::mo_release" never resolves
  // to. Point both symbols at the one record, so the global emitted for the template is the one an instantiation loads.
  for (auto const &scoped_sym : new_sup_scope_ptr->AllVarSymbols(true)) {
    if (const auto old_sym = old_sup_scope.GetVarSymbol(scoped_sym->Name.get(), true); old_sym != nullptr) {
      scoped_sym->LlvmInfo = old_sym->LlvmInfo;
    }
  }

  // 2. Attach the instantiation to the scope tree, beside the template it was cloned from.
  old_sup_scope.Parent->Children.EmplaceBack(std::move(new_sup_scope));

  // 3. Bind the generic parameters, against the instantiation itself.
  auto tm = ScopeManager(sm->GlobalScope, new_sup_scope_ptr);
  RegisterGenericSyms(generic_args.Args, new_sup_scope_ptr, &tm, meta);
  AddSelfTypeSym(*new_sup_scope_ptr, &new_cls_scope);

  // 4. Substitute the bindings into what the clone inherited: the block's "type" aliases and its "cmp" constants.
  for (auto const &scoped_sym : new_sup_scope_ptr->AllTypeSymbols(true)) {
    if (scoped_sym->Alias == nullptr) { continue; }
    auto old_type_sub = scoped_sym->Alias->Resolved->SubstituteGenerics(generic_args.GetAllArgs());
    // old_type_sub->Stage7_AnalyseSemantics(&tm, meta);  // Todo: Why is this commented?
    const auto old_type_sub_sym = new_sup_scope_ptr->GetTypeSymbol(old_type_sub.get());

    // Its own description rather than the one it was cloned holding: that one still describes the template, and is
    // shared with it, so substituting into it in place would rewrite the template's own meaning.
    auto substituted = MakeShared<AliasInfo>(*scoped_sym->Alias);
    substituted->Resolved = std::move(old_type_sub);
    substituted->DeclScope = new_sup_scope_ptr;
    scoped_sym->Alias = std::move(substituted);

    if (old_type_sub_sym != nullptr) {
      // Stamped with what it resolved to here, as the template's target is ("TypeStatementAst::Stage4_ResolveDeclarations").
      auto const &resolved = scoped_sym->Alias->Resolved;
      if (resolved->Stamp() == nullptr and old_type_sub_sym->Kind == TypeKind::Class
        and old_type_sub_sym->Alias == nullptr) { resolved->SetStamp(old_type_sub_sym); }
      scoped_sym->LlvmInfo = old_type_sub_sym->LlvmInfo;
      scoped_sym->Type = old_type_sub_sym->Type;
      scoped_sym->LinkedScope = old_type_sub_sym->LinkedScope;
      scoped_sym->InvalidateFqNameCache();
    }
  }
  SubstituteVarSymTypes(*new_sup_scope_ptr, generic_args.GetAllArgs(), &tm, meta);

  // Create the scope for the new super class type. This will handle recursive sup-scope creation.
  auto super_cls_scope = static_cast<Scope*>(nullptr);
  if (const auto ext_ast = AstAs<SupPrototypeExtensionAst>(old_sup_scope.AstNode); ext_ast != nullptr) {
    const auto new_fq_super_type = ext_ast->SuperClass->SubstituteGenerics(generic_args.GetAllArgs());
    AnalyseSubstitutedType(*new_fq_super_type, &tm, meta, true, true);

    // Resolved where it was analysed: a super class naming a generic the instantiation carries is registered with
    // that generic, which the class being attached to has no path to.
    const auto super_cls_sym = new_sup_scope_ptr->GetTypeSymbol(new_fq_super_type.get());
    super_cls_scope = super_cls_sym != nullptr ? super_cls_sym->LinkedScope : nullptr;
  }

  return {new_sup_scope_ptr, super_cls_scope};
}

auto spp::analyse::utils::monomorphization::PotentiallyGenerateGenericSubstitutedPrototype(
  FunctionPrototypeAst *fn_proto,
  Scope const *fn_scope,
  GenericArgumentGroupAst &combined_generics,
  Shared<TypeAst> const &variadic_pack_type,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Tup<FunctionPrototypeAst*, Scope const*> {
  //
  using errors::SppSecondClassBorrowViolationError;
  using monomorphization::CreateGenericFunScope;
  using type_predicates::IsTypeBorrowed;

  // Inference has already produced a binding for every one of this prototype's generic parameters, including the
  // ones it inherited from the enclosing "sup" block.
  NormaliseGenericArgs(combined_generics, CalleeScope(*fn_proto, *fn_scope), sm);

  // Stamp comp arguments naming a comp parameter with that parameter, where they are written. The instantiation
  // below binds the callee's own generics in the same table, and a caller's "w" read there by name would be the
  // callee's inherited "w" - "U32::from(SizedIntegerUnsigned[w]::from(..))" inside BigUInt's "sup [cmp w: U32]".
  for (auto const *arg : combined_generics.GetCompArgs()) {
    if (arg->CompVal != nullptr) { comp_generics::StampCompGenerics(*arg->CompVal, *sm->CurrentScope); }
  }

  // Separate variadic instantiation by the types going into the
  // variadic function parameter.
  if (variadic_pack_type != nullptr) {
    auto pack_name = MakeUnique<TypeIdentifierAst>(
      variadic_pack_type->PosStart(),
      packs::PackTypeParamName(*fn_proto->FnParamGroup->GetVariadicParams()->ExtractName()),
      nullptr);
    combined_generics.Args.EmplaceBack(GenericArgumentAst::NewType(
      std::move(pack_name), AstClone(variadic_pack_type)));
  }

  // Consider if we need to create a generic substituted
  // function prototype.
  if (not combined_generics.Args.IsEmpty()) {
    // Reuse the instantiation for arguments of this identity if one already exists - what they resolve to from the
    // call site, not how they are spelled.
    const auto identity = sm->CurrentScope->InstanceIdentityKey(combined_generics.GetAllArgs());
    if (auto [existing_scope, existing_proto] = fn_proto->FindGenericSubstitution(identity);
      existing_proto != nullptr) {
      return {existing_proto, existing_scope};
    }

    auto new_fn_proto = AstClone(fn_proto);
    new_fn_proto->SetNonGenericImpl(fn_proto);
    new_fn_proto->DetachLlvmFuncSlot();

    // Create the new function scope for the generic implementation.
    const auto new_fn_scope = CreateGenericFunScope(
      *fn_scope, GenericArgumentGroupAst(nullptr, AstCloneVec(combined_generics.Args), nullptr), sm, meta);
    auto tm = ScopeManager(sm->GlobalScope, new_fn_scope);

    // Drop only the parameters this substitution actually bound.
    new_fn_proto->GnParamGroup->Params |= genex::actions::remove_if([&](auto const &param) {
      return genex::any_of(combined_generics.Args, [&](auto const &arg) {
        return arg->ViewName() == param->Name->ToString();
      });
    });

    auto &generic_sub_slot = AstBody(
      fn_scope->AstNode)[0]->To<FunctionPrototypeAst>()->RegisteredGenericSubstitutions().back();
    generic_sub_slot.IdentityKey = identity;

    // Substitute and analyse the function parameters and return
    // type.
    for (auto *p : new_fn_proto->FnParamGroup->GetNonSelfParams()) {
      p->Type = p->Type->SubstituteGenerics(combined_generics.GetAllArgs());
      p->Type->Stage7_AnalyseSemantics(&tm, meta);

      // The default is checked against the substituted type, so it is rebuilt from what was written, in the same
      // terms. The template's copy carries the template's analysis: "Wrap[T]::new()" would still return "Wrap[T=T]".
      if (auto *const opt = p->To<FunctionParameterOptionalAst>(); opt != nullptr and opt->Source.OriginalDefaultVal != nullptr) {
        opt->DefaultVal = AstClone(opt->Source.OriginalDefaultVal->SubstituteGenericsExpr(combined_generics.GetAllArgs()));
      }
    }

    ReattachCallableConstraints(*new_fn_proto, *fn_proto, new_fn_scope, combined_generics, tm, meta);
    RetypeInheritedSymbols(*new_fn_proto, new_fn_scope, combined_generics, variadic_pack_type, tm, meta);

    new_fn_proto->ReturnType = new_fn_proto->ReturnType->SubstituteGenerics(combined_generics.GetAllArgs());
    new_fn_proto->ReturnType->Stage7_AnalyseSemantics(&tm, meta);

    // Check the new return type isn't a borrow type.
    RaiseIf<SppSecondClassBorrowViolationError>(
      IsTypeBorrowed(*new_fn_proto->ReturnType, tm),
      {sm->CurrentScope},
      ERR_ARGS(*new_fn_proto->ReturnType, *new_fn_proto->ReturnType, "substituted function return type"));

    generic_sub_slot.IsConcrete = ComputeIsConcrete(
      combined_generics, *new_fn_proto, new_fn_scope, tm, sm, meta);

    // Save the generic implementation against the base function,
    // and update the active scope and prototype.
    const auto new_fn_proto_ptr = new_fn_proto.get();
    generic_sub_slot.Proto = std::move(new_fn_proto);
    return {new_fn_proto_ptr, new_fn_scope};
  }

  return {fn_proto, fn_scope};
}

auto spp::analyse::utils::monomorphization::InstantiateOverload(
  FunctionPrototypeAst *fn_proto,
  Scope const *fn_scope,
  GenericArgumentGroupAst &generic_args,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> FunctionPrototypeAst* {
  // The arguments arrive already named - they are read off a "sup" block that has bound them - so there is nothing
  // here for inference to do, and the substitution itself is the whole of what a call site would reach.
  return std::get<0>(PotentiallyGenerateGenericSubstitutedPrototype(
    fn_proto, fn_scope, generic_args, nullptr, sm, meta));
}

auto spp::analyse::utils::monomorphization::FindInstantiatedOverload(
  FunctionPrototypeAst *fn_proto,
  GenericArgumentGroupAst &generic_args,
  ScopeManager const *sm)
  -> FunctionPrototypeAst* {
  // Nothing to substitute means the template is the only prototype there is, exactly as
  // "PotentiallyGenerateGenericSubstitutedPrototype" decides it.
  NormaliseGenericArgs(generic_args, CalleeScope(*fn_proto, *sm->CurrentScope), sm);
  if (generic_args.Args.IsEmpty()) { return fn_proto; }
  return fn_proto->FindGenericSubstitution(sm->CurrentScope->InstanceIdentityKey(generic_args.GetAllArgs())).second;
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
    if (genex::any_of(s->AllTypeSymbols(true), [](auto const *sym) { return sym->Kind == TypeKind::GenericParam; })) {
      return true;
    }
    if (genex::any_of(s->AllVarSymbols(true), [](auto const *sym) {
      return sym->Kind == VariableKind::GenericCompParam;
    })) { return true; }
  }
  return false;
}
