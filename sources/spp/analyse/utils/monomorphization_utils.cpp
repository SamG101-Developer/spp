module;
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.analyse.utils.monomorphization_utils;
import spp.asts.generic_parameter_ast;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.utils.algorithms;
import spp.analyse.utils.type_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
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
import spp.utils.ptr;
import genex;
import std;

auto spp::analyse::utils::monomorphization_utils::CreateGenericClsScope(
  asts::TypeIdentifierAst &type_part,
  Shared<scopes::TypeSymbol> const &old_cls_sym,
  Vec<Shared<scopes::Symbol>> const &external_generic_syms,
  const bool is_tuple,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> scopes::Scope* {
  // Determine the old class scope, and create a new scope
  // for the generic substituted type. Same scope parent and
  // scope node.
  const auto old_cls_scope = old_cls_sym->LinkedScope ? : old_cls_sym->ScopeDefinedIn;
  const auto name_clone = asts::AstCloneShared(&type_part);
  auto [new_cls_scope, new_cls_scope_ptr] = MakeUniqueAndRaw<scopes::Scope>(
    scopes::ScopeTypeIdentifierName(name_clone),
    old_cls_scope->Parent, old_cls_scope->AstNode);

  // Create a new class symbol, based on the new class scope,
  // and copy over important information.
  const auto new_cls_sym = MakeShared<scopes::TypeSymbol>(
    name_clone, new_cls_scope->AstNode->To<asts::ClassPrototypeAst>(), new_cls_scope.get(), sm->CurrentScope,
    old_cls_scope->Parent, old_cls_sym->IsGeneric, old_cls_sym->IsDirectlyCopyable, old_cls_sym->Visibility);

  // The instantiation is copyable if the template it
  // substitutes is. Also copy over the "zero-type" info.
  new_cls_sym->CopyableBaseSym = old_cls_sym;
  new_cls_sym->ZeroTypeBaseSym = old_cls_sym;

  // Handle the possible "alias" logic. If there is an
  // alias statement, clone it for modification.
  auto new_alias_stmt = asts::AstClone(old_cls_sym->AliasStmt);
  if (new_alias_stmt) {
    new_alias_stmt->MappedOldType = new_alias_stmt->MappedOldType->SubstituteGenerics(
      type_part.GnArgGroup->GetAllArgs());
    new_alias_stmt->OldType = new_alias_stmt->MappedOldType;
    new_alias_stmt->OldType->Stage7_AnalyseSemantics(sm, meta);
    // TODO: Remove generic parameters that have been given arguments (not always all generic args).
    //  Move the argument filter out of the recursive alias searcher and reuse it here.

    const auto target_scope = new_alias_stmt->GetAstScope()->Parent;
    target_scope->AddTypeSymbol(new_cls_sym);
    new_alias_stmt->_TrackingScope->AddTypeSymbol(new_cls_sym);
    new_alias_stmt->_TrackingScope->Children.EmplaceBack(std::move(new_cls_scope));
    new_cls_sym->AliasStmt = std::move(new_alias_stmt);
  }

  // Configure the new scope based on the base (old) scope.
  else {
    new_cls_scope->Parent->AddTypeSymbol(new_cls_sym);
    new_cls_scope->Parent->Children.EmplaceBack(std::move(new_cls_scope));
  }
  new_cls_scope_ptr->TySym = new_cls_sym;
  new_cls_scope_ptr->InternalTable = old_cls_scope->InternalTable;
  new_cls_scope_ptr->NonGenericScope = old_cls_scope;

  if (meta->CurrentStage > 7) {
    sm->AttachSpecificSuperScopes(*new_cls_scope_ptr, meta);
  }

  // No more checks for tuples. This is needed to prevent
  // recursive checks on the generics (variadics would become
  // tuples, infinitely).
  auto new_ast = asts::AstClone(
    old_cls_scope->AstNode->To<asts::ClassPrototypeAst>());
  new_ast->SetAstScope(new_cls_scope_ptr);
  new_ast->GnParamGroup->Params.Clear();
  const auto new_ast_ptr = new_ast.get();

  old_cls_sym->Type->RegisterGenericSubstitution(
    new_cls_scope_ptr, std::move(new_ast));
  if (is_tuple) {
    return new_cls_scope_ptr;
  }

  // Register the generic symbols.
  RegisterGenericSyms(
    external_generic_syms, type_part.GnArgGroup->Args,
    new_cls_scope_ptr, sm, meta);

  // Run generic substitution on the symbols in the scope.
  const auto fq_type = new_cls_sym->FqName();
  auto substitution_generics = fq_type->LastTypePart()->GnArgGroup->GetAllArgs();
  substitution_generics.AppendRange(type_part.GnArgGroup->GetAllArgs());

  // Substitute the "Self" sym.
  const auto self_type = asts::AstName(old_cls_scope->AstNode)->SubstituteGenerics(type_part.GnArgGroup->GetAllArgs());
  const auto new_self_sym = MakeShared<scopes::TypeSymbol>(
    MakeUnique<asts::TypeIdentifierAst>(0uz, "Self", nullptr), sm->SelfProto(), new_cls_scope_ptr,
    new_cls_scope_ptr);
  new_cls_scope_ptr->AddTypeSymbol(new_self_sym);

  auto tm = scopes::ScopeManager(
    sm->GlobalScope,
    new_alias_stmt ? sm->CurrentScope->GetTypeSymbol(new_alias_stmt->OldType.get())->LinkedScope : new_cls_scope_ptr);
  for (auto const &scoped_sym : new_cls_scope_ptr->AllVarSymbols(true)) {
    scoped_sym->Type = scoped_sym->Type->SubstituteGenerics(substitution_generics);
    if (meta->CurrentStage > 5) {
      meta->Save();
      meta->AllowAbstractType = true;
      scoped_sym->Type->Stage7_AnalyseSemantics(&tm, meta);
      meta->Restore();
    }
  }

  for (auto *attr : new_ast_ptr->Impl->Members
       | genex::views::ptr
       | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
    //
    attr->Type = attr->Type->SubstituteGenerics(substitution_generics);
    if (meta->CurrentStage > 5) {
      attr->Stage7_AnalyseSemantics(&tm, meta);
    }

    // Remove void attributes from the class.
    if (type_utils::IsTypeVoid(*attr->Type, *new_cls_scope_ptr)) {
      new_cls_scope_ptr->RemVarSymbol(attr->Name.get());
      new_ast_ptr->Impl->Members |= genex::actions::remove_if([&](auto &&x) { return x.get() == attr; });
    }
  }

  // Return the new class scope.
  return new_cls_scope_ptr;
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericFunScope(
  scopes::Scope const &old_fun_scope,
  asts::GenericArgumentGroupAst const &generic_args,
  Vec<Shared<scopes::Symbol>> const &external_generic_syms,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> scopes::Scope* {
  // Create a new scope and symbol for the generic substituted
  // function.
  auto [new_fun_scope, new_fun_scope_ptr] = MakeUniqueAndRaw<scopes::Scope>(old_fun_scope);
  const auto old_fn_proto = asts::AstBody(old_fun_scope.AstNode)[0]->To<asts::FunctionPrototypeAst>();
  old_fn_proto->RegisterGenericSubstitution(
    std::move(new_fun_scope), nullptr,
    MakeUnique<asts::GenericArgumentGroupAst>(nullptr, asts::AstCloneVec(generic_args.Args), nullptr));

  // "Scope"'s copy constructor copies symbol tables shallowly, so the clone and the template share their symbol
  // objects outright. Deep-copy them across the cloned subtree before anything mutates one, or substituting a
  // parameter's type below would rewrite the template's own symbol and leave every other instantiation - and the
  // template itself - seeing this instantiation's types.
  const auto deep_copy_scope_syms = [](auto const &self, scopes::Scope *dst, scopes::Scope const *src) -> void {
    dst->InternalTable = src->InternalTable;
    for (auto i = 0uz; i < dst->Children.Len() and i < src->Children.Len(); ++i) {
      self(self, dst->Children[i].get(), src->Children[i].get());
    }
  };
  deep_copy_scope_syms(deep_copy_scope_syms, new_fun_scope_ptr, &old_fun_scope);

  auto tm = scopes::ScopeManager(sm->GlobalScope, new_fun_scope_ptr);
  RegisterGenericSyms(external_generic_syms, generic_args.Args, new_fun_scope_ptr, &tm, meta);

  // The scope was cloned from the template, so its symbols still hold the template's types: "that" in "fun from(that:
  // SizedIntegerUnsigned[w])" keeps a type naming "w" as a parameter rather than the value bound here. Nothing else
  // re-derives them - the caller substitutes the prototype's parameter ASTs, but a symbol's type is what "InferType"
  // hands back verbatim, so a call in the instantiated body infers "w" from "w" and resolves against the template
  // instead of this instantiation. "CreateGenericClsScope" does the same for class attributes.
  //
  // Applied to the whole cloned subtree, not just the root: the scope handed in here is the mock "sup" scope wrapping
  // the function (its ast node is the "sup" block, whose first member is the prototype - see the caller), so the
  // parameter symbols live one level down, and the body's own locals another level below that.
  const auto substitution_generics = generic_args.GetAllArgs();
  const auto substitute_scope_syms = [&](auto const &self, scopes::Scope *scope, const bool is_root) -> void {
    // "RegisterGenericSyms" bound the generics on the root, but a function written inside a generic "sup" block
    // declares that block's parameters as its own (see "FunctionPrototypeAst::Stage1_PreProcess"), so the function
    // scope one level down holds unbound symbols of the very same names. A lookup from the body reaches those first
    // and never sees the binding, leaving "SizedIntegerUnsigned[w]" symbolic in an instantiation that knows exactly
    // what "w" is. Drop the shadowing copies so the names resolve to what this instantiation bound them to; the
    // subtree is private to it, and the declaration itself still lives on the prototype.
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
    for (auto const &scoped_sym : scope->AllVarSymbols(true)) {
      if (scoped_sym->Type == nullptr) { continue; }
      scoped_sym->Type = scoped_sym->Type->SubstituteGenerics(substitution_generics);
      if (meta->CurrentStage > 5) {
        meta->Save();
        meta->AllowAbstractType = true;
        scoped_sym->Type->Stage7_AnalyseSemantics(&stm, meta);
        meta->Restore();
      }
    }
    for (auto const &child : scope->Children) { self(self, child.get(), false); }
  };
  substitute_scope_syms(substitute_scope_syms, new_fun_scope_ptr, true);

  // Return the new function scope.
  return new_fun_scope_ptr;
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericSupScope(
  scopes::Scope &old_sup_scope,
  scopes::Scope &new_cls_scope,
  asts::GenericArgumentGroupAst const &generic_args,
  Vec<Shared<scopes::Symbol>> const &external_generic_syms,
  scopes::ScopeManager const *sm,
  asts::meta::CompilerMetaData *meta)
  -> std::tuple<scopes::Scope*, scopes::Scope*> {
  // Create a new scope for the generic substituted super scope.
  const auto self_type = asts::AstName(old_sup_scope.AstNode)->SubstituteGenerics(generic_args.GetAllArgs());
  auto new_sup_scope = MakeUnique<scopes::Scope>(old_sup_scope);
  auto new_sup_scope_ptr = new_sup_scope.get();
  new_sup_scope_ptr->InternalTable = old_sup_scope.InternalTable;
  old_sup_scope.Parent->Children.EmplaceBack(std::move(new_sup_scope));

  std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->Name).Name =
    SubstituteSupScopeName(std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->Name).Name, generic_args);

  // Register the generic symbols.
  auto tm = scopes::ScopeManager(sm->GlobalScope, new_sup_scope_ptr);
  RegisterGenericSyms(external_generic_syms, generic_args.Args, new_sup_scope_ptr, &tm, meta);

  meta->Save();
  meta->IgnoreAccessModifierViolations = true;
  self_type->Stage7_AnalyseSemantics(&tm, meta);
  meta->Restore();
  const auto new_self_sym = MakeShared<scopes::TypeSymbol>(
    MakeUnique<asts::TypeIdentifierAst>(0uz, "Self", nullptr), sm->SelfProto(), &new_cls_scope,
    new_sup_scope_ptr);
  new_sup_scope_ptr->AddTypeSymbol(new_self_sym);

  // Run generic substitution on the aliases in the new scope.
  for (auto const &scoped_sym : new_sup_scope_ptr->AllTypeSymbols(true)) {
    if (scoped_sym->AliasStmt != nullptr) {
      auto old_type_sub = scoped_sym->AliasStmt->OldType->SubstituteGenerics(generic_args.GetAllArgs());
      // old_type_sub->Stage7_AnalyseSemantics(&tm, meta);  // Todo: Why is this commented?
      const auto old_type_sub_sym = new_sup_scope_ptr->GetTypeSymbol(old_type_sub.get());

      scoped_sym->AliasStmt->OldType = std::move(old_type_sub);
      scoped_sym->AliasStmt->MappedOldType = scoped_sym->AliasStmt->OldType;
      if (scoped_sym->AliasStmt->GetAstScope()) {
        // Self doesn't have a scope on it
        scoped_sym->AliasStmt->GetAstScope()->Parent = new_sup_scope_ptr;
      }

      if (old_type_sub_sym != nullptr) {
        old_type_sub_sym->AliasedBySyms.PushBack(scoped_sym->SharedFromThis<scopes::TypeSymbol>());
        scoped_sym->Type = old_type_sub_sym->Type;
        scoped_sym->LinkedScope = old_type_sub_sym->LinkedScope;
      }
    }
  }

  // Run generic substitution on the constants in the new scope.
  for (auto const &scoped_sym : new_sup_scope_ptr->AllVarSymbols(true)) {
    auto old_type_sub = scoped_sym->Type->SubstituteGenerics(generic_args.GetAllArgs());
    scoped_sym->Type = std::move(old_type_sub);

    // A "cmp" on a generic "sup" block mangles to "<module>#<name>": "mangle_mod_name" drops every "<...>" scope, so
    // neither the block nor its generic arguments reach the name, and one written constant is one global however many
    // instantiations name it. The symbol table copied above is a deep copy, so this instantiation holds its own symbol
    // with an empty llvm record, and stage 10 only ever walks the template - "Atom[Bool]::mo_release" would resolve to
    // a symbol nothing ever gives storage to. Point both symbols at the one record instead, so the global stage 10
    // emits for the template is the one an instantiation loads.
    if (const auto old_sym = old_sup_scope.GetVarSymbol(scoped_sym->Name.get(), true); old_sym != nullptr) {
      scoped_sym->LlvmInfo = old_sym->LlvmInfo;
    }
  }

  // Create the scope for the new super class type. This will handle recursive sup-scope creation.
  auto super_cls_scope = static_cast<scopes::Scope*>(nullptr);
  if (const auto ext_ast = old_sup_scope.AstNode->To<asts::SupPrototypeExtensionAst>(); ext_ast != nullptr) {
    const auto new_fq_super_type = ext_ast->SuperClass->SubstituteGenerics(generic_args.GetAllArgs());
    meta->Save();
    meta->AllowAbstractType = true;
    meta->IgnoreAccessModifierViolations = true;
    new_fq_super_type->Stage7_AnalyseSemantics(&tm, meta);
    meta->Restore();
    super_cls_scope = new_cls_scope.GetTypeSymbol(new_fq_super_type.get())->LinkedScope;
  }

  return std::make_tuple(new_sup_scope_ptr, super_cls_scope);
}

auto spp::analyse::utils::monomorphization_utils::CreateGenericSym(
  asts::GenericArgumentAst const &generic,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta,
  scopes::ScopeManager *tm)
  -> Shared<scopes::Symbol> {
  //
  using errors::SppInternalCompilerError;

  // Handle the generic type argument => creates a type symbol.
  if (const auto type_arg = generic.To<asts::GenericArgumentTypeKeywordAst>(); type_arg != nullptr) {
    // "Self" should not be looked up and changed.
    if (type_arg->Val->IsSelfType()) {
      return MakeShared<scopes::TypeSymbol>(
        type_arg->Name->TypeParts().Back(), nullptr, nullptr, sm.CurrentScope, sm.CurrentScope->ParentModule(), true);
    }

    const auto true_val_sym = sm.CurrentScope->GetTypeSymbol(type_arg->Val.get());

    // Build the type symbol for the generic type argument.
    auto sym = MakeShared<scopes::TypeSymbol>(
      type_arg->Name->TypeParts().Back(), true_val_sym ? true_val_sym->Type : nullptr,
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
      sm.CurrentScope, // or tm?
      false, true, asts::utils::Visibility::kPublic);
    sym->MemInfo->AstCompTime = asts::AstClone(comp_arg);
    return sym;
  }

  Raise<SppInternalCompilerError>(
    {sm.CurrentScope},
    ERR_ARGS(generic, "Unknown generic argument ast type"));
}

auto spp::analyse::utils::monomorphization_utils::RegisterGenericSyms(
  Vec<Shared<scopes::Symbol>> const &external_generic_syms,
  Vec<Unique<asts::GenericArgumentAst>> const &generic_args,
  scopes::Scope *scope,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> void {
  // Register the type symbols to the scope.
  for (auto const &e : external_generic_syms | spp::views::cast_shared<scopes::TypeSymbol>()) {
    scope->AddTypeSymbol(e);
  }

  // Register the variable symbols to the scope.
  for (auto const &e : external_generic_syms | spp::views::cast_shared<scopes::VariableSymbol>()) {
    scope->AddVarSymbol(e);
  }

  // Convert the generic arguments into symbols.
  auto generic_syms = generic_args
    | genex::views::transform([&](auto const &g) { return CreateGenericSym(*g, *sm, meta); })
    | genex::to<Vec>();

  // Register the created generic symbols to the scope.
  for (auto const &e : generic_syms | spp::views::cast_shared<scopes::TypeSymbol>()) {
    const auto old = scope->RemTypeSymbol(e->Name.get());
    if (old) { e->GenericConstraints = asts::AstCloneVecShared(old->GenericConstraints); }
    scope->AddTypeSymbol(e);
  }

  // Register the created generic symbols to the scope.
  for (auto const &e : generic_syms | spp::views::cast_shared<scopes::VariableSymbol>()) {
    scope->RemVarSymbol(e->Name.get());
    scope->AddVarSymbol(e);
  }
}

auto spp::analyse::utils::monomorphization_utils::SubstituteSupScopeName(
  Str const &old_sup_scope_name,
  asts::GenericArgumentGroupAst const &generic_args)
  -> Str {
  const auto parts = old_sup_scope_name
    | genex::views::split('#')
    | genex::to<Vec>()
    | genex::views::transform([](auto &&x) { return Str(x.begin(), x.end()); })
    | genex::to<Vec>();

  if (not parts[1].contains(" ext ")) {
    const auto t = INJECT_CODE(parts[1], parse_type_expression)->SubstituteGenerics(generic_args.GetAllArgs());
    const auto o = parts[0] + "#" + t->ToString() + "#" + parts[2];
    return o;
  }
  const auto t = INJECT_CODE(parts[1].substr(0, parts[1].find(" ext ")), parse_type_expression)->SubstituteGenerics(
    generic_args.GetAllArgs());
  const auto u = INJECT_CODE(parts[1].substr(parts[1].find(" ext ") + 5), parse_type_expression)->SubstituteGenerics(
    generic_args.GetAllArgs());
  const auto o = parts[0] + "#" + t->ToString() + " ext " + u->ToString() + "#" + parts[2];

  return o;
}
