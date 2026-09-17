module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.monomorphization_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.instantiation_queue;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.codegen.llvm_ctx;
import spp.utils.algorithms;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::monomorphization_utils {
  namespace {
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
        if (auto folded = cmp_utils::FoldCompExpr(*arg->CompVal, scope); folded != nullptr) {
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

      // Handle the generic type argument => creates a type symbol.
      if (generic.Name != nullptr and generic.TypeVal != nullptr) {
        // "Self" should not be looked up and changed.
        if (generic.TypeVal->IsSelfType()) {
          return MakeShared<TypeSymbol>(
            NameLastTypePart(*generic.Name), nullptr, nullptr, sm.CurrentScope, sm.CurrentScope->ParentModule(),
            TypeKind::GenericArg);
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
          true_val_sym ? true_val_sym->LinkedScope : nullptr, sm.CurrentScope, sm.CurrentScope->ParentModule(),
          TypeKind::GenericArg,
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
      if (generic.Name != nullptr and generic.CompVal != nullptr) {
        auto sym = MakeShared<VariableSymbol>(
          IdentifierAst::FromType(*generic.Name),
          generic.CompVal->InferType(tm ? tm : &sm, meta),
          sm.CurrentScope,
          VariableKind::GenericCompArg, false, asts::utils::Visibility::kPublic);
        // A comp argument naming a bound comp generic binds what that is bound to where it is written, as a type
        // argument binds the type it names there ("SizedInteger[w]" in a "[cmp w: U32]" instance binds 32, not "w").
        // A closed value binds what it folds to ("n + 1_uz" with "n" bound to "1_uz" binds "2_uz").
        auto value = cmp_utils::FoldCompExpr(*generic.CompVal, *sm.CurrentScope);
        if (value == nullptr) {
          value = AstClone(generic.CompVal);
          if (auto const *const id = generic.CompVal->To<IdentifierAst>(); id != nullptr) {
            if (auto const *const var = sm.CurrentScope->GetVarSymbol(id); var != nullptr) {
              if (auto const *const bound = var->BoundCompValue(); bound != nullptr) { value = AstClone(bound); }
            }
          }
        }
        sym->CompTimeValue = std::move(value);
        return sym;
      }

      Raise<SppInternalCompilerError>(
        {sm.CurrentScope},
        ERR_ARGS(generic, "Unknown generic argument ast type"));
      std::unreachable();
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
          binding.BindsParamId = param->ParamId != 0 ? param->ParamId : param->BindsParamId;
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
      meta::CompilerMetaData *meta)
      -> void {
      for (auto const &scoped_sym : scope.AllVarSymbols(true)) {
        if (scoped_sym->Type == nullptr) { continue; }

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

auto spp::analyse::utils::monomorphization_utils::InstantiateForScope(
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

auto spp::analyse::utils::monomorphization_utils::MonomorphiseToFixedPoint(
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Every instantiation registered during stages 1-9 is already waiting, because registering one is what records its
  // template. Draining a template analyses whatever it has accumulated since, and that analysis records whatever it
  // reaches in turn, so the loop ends exactly when nothing new was found.
  while (auto *fn_template = instantiation_queue::Pop()) {
    fn_template->AnalysePendingGenericSubstitutions(sm, meta);
  }
  sm->Reset();
}

auto spp::analyse::utils::monomorphization_utils::CanonicaliseGenericArgs(
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
    if (cmp_utils::FoldCompExpr(*arg->CompVal, scope) != nullptr) { continue; }
    const auto *const val_ident = arg->CompVal->To<IdentifierAst>();
    if (val_ident == nullptr) { continue; }
    const auto *const val_sym = scope.GetVarSymbol(val_ident);
    if (const auto *const bound = val_sym != nullptr ? val_sym->BoundCompValue() : nullptr; bound != nullptr) {
      arg->CompVal = AstClone(bound);
    }
  }
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericClsScope(
  TypeIdentifierAst &type_part,
  Shared<TypeSymbol> const &old_cls_sym,
  const bool is_tuple,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Scope* {
  // 0. Stamp the arguments with what they mean where they are written, before anything below copies them.
  for (auto const *arg : type_part.GnArgGroup->GetTypeArgs()) {
    if (arg->TypeVal != nullptr) { type_utils::StampWrittenParts(*arg->TypeVal, *sm->CurrentScope); }
  }
  for (auto const *arg : type_part.GnArgGroup->GetCompArgs()) {
    if (arg->CompVal != nullptr) { cmp_utils::StampCompGenerics(*arg->CompVal, *sm->CurrentScope); }
  }

  // The identity the instantiation is filed under, read where it is written, like its arguments.
  const auto identity_key = sm->CurrentScope->InstanceIdentityKey(type_part.GnArgGroup->GetAllArgs());

  // 1. Clone the template's scope. A class is the one construct whose instantiation gets a fresh scope rather than a
  // copy: its name is the instantiated type, not the template's, so only the symbols are carried over.
  const auto old_cls_scope = old_cls_sym->LinkedScope ? : old_cls_sym->ScopeDefinedIn;
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
    old_cls_scope->Parent, old_cls_sym->Kind, old_cls_sym->IsDirectlyCopyable, old_cls_sym->Visibility);
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
      attr->Stage7_AnalyseSemantics(&tm, meta);
    }
  }

  return new_cls_scope_ptr;
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericFunScope(
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

  // 4. Substitute the bindings into what the clone inherited, over the whole subtree.
  const auto substitution_generics = generic_args.GetAllArgs();
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
    SubstituteVarSymTypes(*scope, substitution_generics, &stm, meta);
    for (auto const &child : scope->Children) { self(self, child.get(), false); }
  };
  substitute_subtree(substitute_subtree, new_fun_scope_ptr, true);

  return new_fun_scope_ptr;
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericSupScope(
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
