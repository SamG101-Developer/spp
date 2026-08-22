module;
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.analyse.utils.monomorphization_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.instantiation_queue;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.codegen.llvm_ctx;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
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
      asts::TypeAst const &name)
      -> Shared<asts::TypeIdentifierAst> {
      auto *const part = const_cast<asts::TypeAst&>(name).LastTypePart();
      return static_shared_cast<asts::TypeIdentifierAst>(part->shared_from_this());
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
      scopes::Scope &dst,
      scopes::Scope const &src,
      const bool recurse)
      -> void {
      dst.InternalTable.DeepCopyFrom(src.InternalTable);
      if (not recurse) { return; }
      for (auto i = 0uz; i < dst.Children.Len() and i < src.Children.Len(); ++i) {
        GiveScopeOwnSyms(*dst.Children[i].get(), *src.Children[i].get(), true);
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
      asts::GenericArgumentAst const &generic,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData *meta,
      scopes::ScopeManager *tm = nullptr)
      -> Shared<scopes::Symbol> {
      //
      using errors::SppInternalCompilerError;

      // Handle the generic type argument => creates a type symbol.
      if (const auto type_arg = generic.To<asts::GenericArgumentTypeKeywordAst>(); type_arg != nullptr) {
        // "Self" should not be looked up and changed.
        if (type_arg->Val->IsSelfType()) {
          return MakeShared<scopes::TypeSymbol>(
            NameLastTypePart(*type_arg->Name), nullptr, nullptr, sm.CurrentScope, sm.CurrentScope->ParentModule(),
            true);
        }

        const auto true_val_sym = sm.CurrentScope->GetTypeSymbol(type_arg->Val.get());

        // Build the type symbol for the generic type argument.
        auto sym = MakeShared<scopes::TypeSymbol>(
          NameLastTypePart(*type_arg->Name), true_val_sym ? true_val_sym->Type : nullptr,
          true_val_sym ? true_val_sym->LinkedScope : nullptr, sm.CurrentScope, sm.CurrentScope->ParentModule(), true,
          true_val_sym ? true_val_sym->IsDirectlyCopyable : false, asts::utils::Visibility::kPublic,
          asts::AstClone(type_arg->Val->GetConvention()));
        sym->GenericConstraints = true_val_sym
          ? true_val_sym->GenericConstraints
          : decltype(true_val_sym->GenericConstraints){};
        sym->IsDirectlyZeroType = true_val_sym
          ? true_val_sym->IsDirectlyZeroType
          : false;

        // Record what the parameter was bound to. When the value is another (unresolved) generic parameter there is no
        // linked scope to recover the binding from later, so the value type is the only record of it.
        sym->GenericVal = asts::AstCloneShared(type_arg->Val);
        return sym;
      }

      // Handle the generic comp argument => creates a variable symbol.
      if (const auto comp_arg = generic.To<asts::GenericArgumentCompKeywordAst>(); comp_arg != nullptr) {
        auto sym = MakeShared<scopes::VariableSymbol>(
          asts::IdentifierAst::FromType(*comp_arg->Name),
          comp_arg->Val->InferType(tm ? tm : &sm, meta),
          sm.CurrentScope,
          false, true, asts::utils::Visibility::kPublic);
        sym->MemInfo->AstCompTime = asts::AstClone(comp_arg);
        return sym;
      }

      Raise<SppInternalCompilerError>(
        {sm.CurrentScope},
        ERR_ARGS(generic, "Unknown generic argument ast type"));
    }

    /**
     * Bind the generic parameters of an instantiation: register a symbol per generic argument into the instantiation's
     * scope, along with the generic symbols carried in from the scope the instantiation was named in.
     * @param external_generic_syms Generic symbols reachable from the scope the instantiation was named in.
     * @param generic_args The arguments the generic parameters are being bound to.
     * @param scope The instantiation's scope to register the symbols into.
     * @param sm The scope manager the arguments are resolved against. A class resolves them where the instantiation was
     * written, because that is where its argument types are named; a "sup" block or function resolves them against the
     * instantiation itself, so that an argument naming a generic carried in from an enclosing template resolves to the
     * symbol just registered for it.
     * @param meta The compiler meta data.
     */
    auto RegisterGenericSyms(
      SharedVec<scopes::Symbol> const &external_generic_syms,
      UniqueVec<asts::GenericArgumentAst> const &generic_args,
      scopes::Scope *scope,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      // Register the symbols carried in from the enclosing template.
      for (auto const &e : external_generic_syms | spp::views::cast_shared<scopes::TypeSymbol>()) {
        scope->AddTypeSymbol(e);
      }
      for (auto const &e : external_generic_syms | spp::views::cast_shared<scopes::VariableSymbol>()) {
        scope->AddVarSymbol(e);
      }

      // Convert the generic arguments into symbols.
      auto generic_syms = generic_args
        | genex::views::transform([&](auto const &g) { return CreateGenericSym(*g, *sm, meta); })
        | genex::to<Vec>();

      // Register the bindings, replacing the template's own (unbound) parameter symbols of the same names. A type
      // parameter's constraints are written on the template, so they are carried over onto the binding.
      for (auto const &e : generic_syms | spp::views::cast_shared<scopes::TypeSymbol>()) {
        const auto old = scope->RemTypeSymbol(e->Name.get());
        if (old) { e->GenericConstraints = asts::AstCloneVecShared(old->GenericConstraints); }
        scope->AddTypeSymbol(e);
      }
      for (auto const &e : generic_syms | spp::views::cast_shared<scopes::VariableSymbol>()) {
        scope->RemVarSymbol(e->Name.get());
        scope->AddVarSymbol(e);
      }
    }

    /**
     * Analyse a type a substitution has just produced. These types are reached out of the order the writer's own types
     * are, so the checks that assume that order are relaxed for them: an instantiation may name an abstract type before
     * the implementation that satisfies it is attached, and a "sup" block's super class is as visible from the
     * instantiation as it was from the template. The relaxations only ever loosen what the caller already allows.
     * @param type The substituted type to analyse.
     * @param tm The scope manager to analyse it through.
     * @param meta The compiler meta data.
     * @param allow_abstract Whether naming an abstract type is permitted.
     * @param ignore_access Whether access modifiers are ignored.
     */
    auto AnalyseSubstitutedType(
      asts::TypeAst &type,
      scopes::ScopeManager *tm,
      asts::meta::CompilerMetaData *meta,
      const bool allow_abstract,
      const bool ignore_access)
      -> void {
      meta->Save();
      meta->AllowAbstractType = meta->AllowAbstractType or allow_abstract;
      meta->IgnoreAccessModifierViolations = meta->IgnoreAccessModifierViolations or ignore_access;
      type.Stage7_AnalyseSemantics(tm, meta);
      meta->Restore();
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
      scopes::Scope const &scope,
      Vec<asts::GenericArgumentAst*> const &generic_args,
      scopes::ScopeManager *tm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      for (auto const &scoped_sym : scope.AllVarSymbols(true)) {
        if (scoped_sym->Type == nullptr) { continue; }
        scoped_sym->Type = scoped_sym->Type->SubstituteGenerics(generic_args);
        if (meta->CurrentStage > 5) {
          // Note: DO NOT inline "analysed_type", because the scoped_sym->Type
          // can change during the analysis that uses it, leaving the original
          // dereerence pointing to garbage.
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
     * @param sm The scope manager, for the shared "Self" class prototype.
     */
    auto AddSelfTypeSym(
      scopes::Scope &scope,
      scopes::Scope *cls_scope,
      scopes::ScopeManager const &sm)
      -> void {
      scope.AddTypeSymbol(MakeShared<scopes::TypeSymbol>(
        MakeUnique<asts::TypeIdentifierAst>(0uz, "Self", nullptr), sm.SelfProto(), cls_scope, &scope));
    }

    /**
     * The instantiations of generic "sup" blocks built so far, keyed by the template block, the type it is being
     * attached to, and the name the generic arguments substitute into. Building one clones a scope, gives it its own
     * symbols and substitutes every binding into them, and the super-scope attachment pass runs more than once over
     * the same type - once in bulk and again on demand - so without this the same instantiation is built repeatedly
     * and the earlier copies are left orphaned in the scope tree, walked by every traversal that follows.
     */
    auto SupScopeInstantiations()
      -> spp::Map<Str, Tup<scopes::Scope*, scopes::Scope*>>& {
      static auto cache = spp::Map<Str, Tup<scopes::Scope*, scopes::Scope*>>();
      return cache;
    }

    /**
     * Build the key for one instantiation. The two scopes go in as raw bytes rather than as digits, so the key costs
     * one allocation and cannot be ambiguous between a pointer and the name that follows it.
     */
    auto SupScopeInstantiationKey(
      void const *const tmpl,
      void const *const owner,
      StrView name)
      -> Str {
      auto key = Str();
      key.reserve(sizeof(tmpl) + sizeof(owner) + name.size());
      key.append(reinterpret_cast<char const*>(&tmpl), sizeof(tmpl));
      key.append(reinterpret_cast<char const*>(&owner), sizeof(owner));
      key.append(name);
      return key;
    }

    /**
     * Rewrite a "sup" block's scope name for an instantiation, so that the substituted block is named after the
     * arguments it was created for rather than the template's parameters.
     * @param old_sup_scope_name The template block's scope name.
     * @param generic_args The arguments the generic parameters are being bound to.
     * @return The instantiation's scope name.
     */
    auto SubstituteSupScopeName(
      Str const &old_sup_scope_name,
      asts::GenericArgumentGroupAst const &generic_args)
      -> Str {
      const auto parts = old_sup_scope_name
        | genex::views::split('#')
        | genex::to<Vec>()
        | genex::views::transform([](auto &&x) { return Str(x.begin(), x.end()); })
        | genex::to<Vec>();

      // A "sup-functions" block names one type; a "sup-extension" block names the type and its super class.
      if (not parts[1].contains(" ext ")) {
        const auto t = INJECT_CODE(parts[1], parse_type_expression)->SubstituteGenerics(generic_args.GetAllArgs());
        return parts[0] + "#" + t->ToString() + "#" + parts[2];
      }

      const auto t = INJECT_CODE(parts[1].substr(0, parts[1].find(" ext ")), parse_type_expression)
        ->SubstituteGenerics(generic_args.GetAllArgs());
      const auto u = INJECT_CODE(parts[1].substr(parts[1].find(" ext ") + 5), parse_type_expression)
        ->SubstituteGenerics(generic_args.GetAllArgs());
      return parts[0] + "#" + t->ToString() + " ext " + u->ToString() + "#" + parts[2];
    }
  }
}

auto spp::analyse::utils::monomorphization_utils::MonomorphiseToFixedPoint(
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> void {
  // Every instantiation registered during stages 1-9 is already waiting, because registering one is what records its
  // template. Draining a template analyses whatever it has accumulated since, and that analysis records whatever it
  // reaches in turn, so the loop ends exactly when nothing new was found.
  while (auto *fn_template = instantiation_queue::Pop()) {
    fn_template->AnalysePendingGenericSubstitutions(sm, meta);
  }
  sm->Reset();
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericClsScope(
  asts::TypeIdentifierAst &type_part,
  Shared<scopes::TypeSymbol> const &old_cls_sym,
  SharedVec<scopes::Symbol> const &external_generic_syms,
  const bool is_tuple,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> scopes::Scope* {
  // 1. Clone the template's scope. A class is the one construct whose instantiation gets a fresh scope rather than a
  // copy: its name is the instantiated type, not the template's, so only the symbols are carried over.
  const auto old_cls_scope = old_cls_sym->LinkedScope ? : old_cls_sym->ScopeDefinedIn;
  const auto name_clone = asts::AstCloneShared(&type_part);
  auto [new_cls_scope, new_cls_scope_ptr] = MakeUniqueAndRaw<scopes::Scope>(
    scopes::ScopeTypeIdentifierName(name_clone),
    old_cls_scope->Parent, old_cls_scope->AstNode);
  GiveScopeOwnSyms(*new_cls_scope_ptr, *old_cls_scope, false);
  new_cls_scope_ptr->NonGenericScope = old_cls_scope;

  // Create a new class symbol, based on the new class scope, and copy over important information. The instantiation is
  // copyable, and a zero type, exactly when the template it substitutes is.
  const auto new_cls_sym = MakeShared<scopes::TypeSymbol>(
    name_clone, new_cls_scope->AstNode->To<asts::ClassPrototypeAst>(), new_cls_scope.get(), sm->CurrentScope,
    old_cls_scope->Parent, old_cls_sym->IsGeneric, old_cls_sym->IsDirectlyCopyable, old_cls_sym->Visibility);
  new_cls_sym->DerivesFromSym = old_cls_sym;

  new_cls_sym->IsConcrete = genex::all_of(
    type_part.GnArgGroup->Args | genex::views::ptr, [&](auto const *arg) {
      if (const auto type_arg = arg->template To<asts::GenericArgumentTypeAst>(); type_arg != nullptr) {
        return type_utils::IsTypeFullyConcrete(*type_arg->Val, *sm->CurrentScope);
      }
      if (const auto comp_arg = arg->template To<asts::GenericArgumentCompAst>(); comp_arg != nullptr) {
        return comp_arg->Val->template To<asts::IdentifierAst>() == nullptr;
      }
      return true;
    });

  new_cls_scope_ptr->TySym = new_cls_sym;

  // An instantiation of a generic alias is the same alias under substituted arguments, so it gets its own
  // description rather than a clone of the syntax that produced it - only the resolved target actually differs.
  auto new_alias = Shared<scopes::AliasInfo>(nullptr);
  if (old_cls_sym->Alias != nullptr) {
    new_alias = MakeShared<scopes::AliasInfo>(*old_cls_sym->Alias);
    new_alias->Resolved = old_cls_sym->Alias->Resolved->SubstituteGenerics(type_part.GnArgGroup->GetAllArgs());
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

  if (meta->CurrentStage > 7) {
    sm->AttachSpecificSuperScopes(*new_cls_scope_ptr, meta);
  }

  // Register the instantiation's own ast against the template. Its parameter list is emptied, which is what marks it as
  // an instantiation rather than the template it was cloned from.
  auto new_ast = asts::AstClone(old_cls_scope->AstNode->To<asts::ClassPrototypeAst>());
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
  RegisterGenericSyms(
    external_generic_syms, type_part.GnArgGroup->Args, new_cls_scope_ptr, sm, meta);
  AddSelfTypeSym(*new_cls_scope_ptr, new_cls_scope_ptr, *sm);

  // 4. Substitute the bindings into what the clone inherited: the attribute symbols, and the attribute asts they came
  // from. The instantiation's own fully qualified name contributes as well, because an alias binds parameters that the
  // written type does not name.
  const auto fq_type = new_cls_sym->FqName();
  auto substitution_generics = fq_type->LastTypePart()->GnArgGroup->GetAllArgs();
  substitution_generics.AppendRange(type_part.GnArgGroup->GetAllArgs());

  // Todo: an aliased instantiation arguably wants its substituted types read through the scope the alias maps onto,
  //  rather than its own. The branch that did that was dead (it tested a unique_ptr that had already been moved into
  //  the symbol), so it is left out here rather than silently switched on.
  auto tm = scopes::ScopeManager(sm->GlobalScope, new_cls_scope_ptr);
  SubstituteVarSymTypes(*new_cls_scope_ptr, substitution_generics, &tm, meta);

  for (auto *attr : new_ast_ptr->Impl->Members
       | genex::views::ptr
       | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
    //
    attr->Type = attr->Type->SubstituteGenerics(substitution_generics);
    if (meta->CurrentStage > 5) {
      attr->Stage7_AnalyseSemantics(&tm, meta);
    }
  }

  return new_cls_scope_ptr;
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericFunScope(
  scopes::Scope const &old_fun_scope,
  asts::GenericArgumentGroupAst const &generic_args,
  SharedVec<scopes::Symbol> const &external_generic_syms,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> scopes::Scope* {
  // 1. Clone the template's scope. The whole subtree is cloned, because a function's parameters and locals live below
  // the scope handed in here (which is the mock "sup" scope stage 1 lowers the function into), and all of them carry
  // types written in terms of the template's parameters.
  auto [new_fun_scope, new_fun_scope_ptr] = MakeUniqueAndRaw<scopes::Scope>(old_fun_scope);
  GiveScopeOwnSyms(*new_fun_scope_ptr, old_fun_scope, true);

  // 2. Register the instantiation against the template, which takes ownership of its scope. Only the slot is reserved
  // here: the caller fills in the substituted prototype once it has substituted the signature.
  const auto old_fn_proto = asts::AstBody(old_fun_scope.AstNode)[0]->To<asts::FunctionPrototypeAst>();
  old_fn_proto->RegisterGenericSubstitution(
    std::move(new_fun_scope), nullptr,
    MakeUnique<asts::GenericArgumentGroupAst>(nullptr, asts::AstCloneVec(generic_args.Args), nullptr));

  // 3. Bind the generic parameters, against the instantiation itself.
  auto tm = scopes::ScopeManager(sm->GlobalScope, new_fun_scope_ptr);
  RegisterGenericSyms(external_generic_syms, generic_args.Args, new_fun_scope_ptr, &tm, meta);

  // 4. Substitute the bindings into what the clone inherited, over the whole subtree.
  const auto substitution_generics = generic_args.GetAllArgs();
  const auto substitute_subtree = [&](auto const &self, scopes::Scope *scope, const bool is_root) -> void {
    // The bindings were registered on the root, but a function written inside a generic "sup" block declares that
    // block's parameters as its own (see "FunctionPrototypeAst::Stage1_PreProcess"), so the function scope one level
    // down holds unbound symbols of the very same names. A lookup from the body reaches those first and never sees the
    // binding, leaving "SizedIntegerUnsigned[w]" symbolic in an instantiation that knows exactly what "w" is. Drop the
    // shadowing copies so the names resolve to what this instantiation bound them to; the subtree is private to it, and
    // the declaration itself still lives on the prototype.
    if (not is_root) {
      for (auto const &g : generic_args.Args) {
        if (const auto *type_arg = g->To<asts::GenericArgumentTypeKeywordAst>(); type_arg != nullptr) {
          scope->RemTypeSymbol(type_arg->Name->ToUnchecked<asts::TypeIdentifierAst>());
        }
        else if (const auto *comp_arg = g->To<asts::GenericArgumentCompKeywordAst>(); comp_arg != nullptr) {
          scope->RemVarSymbol(asts::IdentifierAst::FromType(*comp_arg->Name).get());
        }
      }
    }

    auto stm = scopes::ScopeManager(sm->GlobalScope, scope);
    SubstituteVarSymTypes(*scope, substitution_generics, &stm, meta);
    for (auto const &child : scope->Children) { self(self, child.get(), false); }
  };
  substitute_subtree(substitute_subtree, new_fun_scope_ptr, true);

  return new_fun_scope_ptr;
}

auto spp::analyse::utils::monomorphization_utils::ClearSupScopeInstantiations()
  -> void {
  SupScopeInstantiations().clear();
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericSupScope(
  scopes::Scope &old_sup_scope,
  scopes::Scope &new_cls_scope,
  asts::GenericArgumentGroupAst const &generic_args,
  SharedVec<scopes::Symbol> const &external_generic_syms,
  scopes::ScopeManager const *sm,
  asts::meta::CompilerMetaData *meta)
  -> Tup<scopes::Scope*, scopes::Scope*> {
  // 0. An instantiation is decided entirely by the template block, the type it is for, and the arguments substituted
  // into its name, so one that has already been built is the answer rather than the seed for another.
  const auto substituted_name = SubstituteSupScopeName(
    std::get<scopes::ScopeBlockName>(old_sup_scope.Name).Name, generic_args);
  const auto instantiation_key = SupScopeInstantiationKey(&old_sup_scope, &new_cls_scope, substituted_name);
  auto &instantiations = SupScopeInstantiations();
  if (const auto hit = instantiations.find(instantiation_key); hit != instantiations.end()) {
    return hit->second;
  }

  // 1. Clone the template's scope. Only the block's own symbols are copied, not its subtree: the members below it are
  // functions, and each is instantiated in its own right by "CreateGenericFunScope" when it is called.
  auto new_sup_scope = MakeUnique<scopes::Scope>(old_sup_scope);
  auto new_sup_scope_ptr = new_sup_scope.get();
  GiveScopeOwnSyms(*new_sup_scope_ptr, old_sup_scope, false);
  std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->Name).Name = substituted_name;

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
  auto tm = scopes::ScopeManager(sm->GlobalScope, new_sup_scope_ptr);
  RegisterGenericSyms(external_generic_syms, generic_args.Args, new_sup_scope_ptr, &tm, meta);

  const auto self_type = asts::AstName(old_sup_scope.AstNode)->SubstituteGenerics(generic_args.GetAllArgs());
  AnalyseSubstitutedType(*self_type, &tm, meta, false, true);
  AddSelfTypeSym(*new_sup_scope_ptr, &new_cls_scope, *sm);

  // 4. Substitute the bindings into what the clone inherited: the block's "type" aliases and its "cmp" constants.
  for (auto const &scoped_sym : new_sup_scope_ptr->AllTypeSymbols(true)) {
    if (scoped_sym->Alias == nullptr) { continue; }
    auto old_type_sub = scoped_sym->Alias->Resolved->SubstituteGenerics(generic_args.GetAllArgs());
    // old_type_sub->Stage7_AnalyseSemantics(&tm, meta);  // Todo: Why is this commented?
    const auto old_type_sub_sym = new_sup_scope_ptr->GetTypeSymbol(old_type_sub.get());

    // Its own description rather than the one it was cloned holding: that one still describes the template, and is
    // shared with it, so substituting into it in place would rewrite the template's own meaning.
    auto substituted = MakeShared<scopes::AliasInfo>(*scoped_sym->Alias);
    substituted->Resolved = std::move(old_type_sub);
    substituted->DeclScope = new_sup_scope_ptr;
    scoped_sym->Alias = std::move(substituted);

    if (old_type_sub_sym != nullptr) {
      old_type_sub_sym->AliasedBySyms.PushBack(scoped_sym->SharedFromThis<scopes::TypeSymbol>());
      scoped_sym->Type = old_type_sub_sym->Type;
      scoped_sym->LinkedScope = old_type_sub_sym->LinkedScope;
      scoped_sym->InvalidateFqNameCache();
    }
  }
  SubstituteVarSymTypes(*new_sup_scope_ptr, generic_args.GetAllArgs(), &tm, meta);

  // Create the scope for the new super class type. This will handle recursive sup-scope creation.
  auto super_cls_scope = static_cast<scopes::Scope*>(nullptr);
  if (const auto ext_ast = old_sup_scope.AstNode->To<asts::SupPrototypeExtensionAst>(); ext_ast != nullptr) {
    const auto new_fq_super_type = ext_ast->SuperClass->SubstituteGenerics(generic_args.GetAllArgs());
    AnalyseSubstitutedType(*new_fq_super_type, &tm, meta, true, true);
    super_cls_scope = new_cls_scope.GetTypeSymbol(new_fq_super_type.get())->LinkedScope;
  }

  const auto result = Tup<scopes::Scope*, scopes::Scope*>{new_sup_scope_ptr, super_cls_scope};
  instantiations.emplace(instantiation_key, result);
  return result;
}
