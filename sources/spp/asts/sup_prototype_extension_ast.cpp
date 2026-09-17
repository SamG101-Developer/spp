module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.sup_prototype_extension_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

namespace spp::asts {
  namespace {
    /**
     * The first block in @p scopes extending a type matching @p name with a super class matching @p super_class. The
     * super class is compared first: it rejects the large majority of candidates for a fraction of the cost of a name
     * comparison, which is the type-symbol lookup this compiler spends most of its time on. The names are then tried
     * both ways round, because a variadic pack binds an argument list only when it sits on the right, so which of two
     * blocks was attached first would otherwise decide whether they are seen to match.
     */
    auto FindMatchingExtension(
      Vec<Scope*> const &scopes,
      TypeAst const &super_class,
      TypeAst const &name,
      Scope const &check_scope,
      const bool check_constraints)
      -> Pair<Scope*, SupPrototypeExtensionAst const*> {
      using analyse::utils::type_compare::GenericInferenceMap;
      using analyse::utils::type_compare::RelaxedTypeEq;
      using analyse::utils::type_compare::TypeEq;
      for (auto *const sc : scopes) {
        const auto ext = AstAs<SupPrototypeExtensionAst>(sc->AstNode);
        if (ext == nullptr or not TypeEq(*ext->SuperClass, super_class, *sc, check_scope, false)) { continue; }
        auto fwd = GenericInferenceMap();
        auto rev = GenericInferenceMap();
        if (RelaxedTypeEq(*ext->Name, name, *sc, check_scope, fwd, false, check_constraints)
          or RelaxedTypeEq(name, *ext->Name, check_scope, *sc, rev, false, check_constraints)) {
          return {sc, ext};
        }
      }
      return {nullptr, nullptr};
    }
  }
}

SPP_MOD_BEGIN
SupPrototypeExtensionAst::SupPrototypeExtensionAst(
  decltype(TokSup) &&tok_sup,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(Name) name,
  decltype(TokExt) &&tok_ext,
  decltype(SuperClass) super_class,
  decltype(Impl) &&impl) :
  TokSup(std::move(tok_sup)),
  GnParamGroup(std::move(generic_param_group)),
  Name(std::move(name)),
  TokExt(std::move(tok_ext)),
  SuperClass(std::move(super_class)),
  Impl(std::move(impl)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokSup, lex::SppTokenType::KW_SUP, "sup");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokExt, lex::SppTokenType::KW_EXT, "ext");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
}

SupPrototypeExtensionAst::~SupPrototypeExtensionAst() = default;

auto SupPrototypeExtensionAst::PosStart() const -> std::size_t {
  // Use the "sup" token.
  return TokSup->PosStart();
}

auto SupPrototypeExtensionAst::PosEnd() const -> std::size_t {
  // Use the superclass.
  return SuperClass->PosEnd();
}

auto SupPrototypeExtensionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<SupPrototypeExtensionAst>(
    AstClone(TokSup),
    AstClone(GnParamGroup),
    AstClone(Name),
    AstClone(TokExt),
    AstClone(SuperClass),
    AstClone(Impl));
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  return ast;
}

auto SupPrototypeExtensionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokSup).append(" ");
  SPP_STRING_APPEND(GnParamGroup).append(GnParamGroup->Params.IsEmpty() ? "" : " ");
  SPP_STRING_APPEND(Name).append(" ");
  SPP_STRING_APPEND(TokExt).append(" ");
  SPP_STRING_APPEND(SuperClass).append(" ");
  SPP_STRING_APPEND(Impl);
  SPP_STRING_END;
}

auto SupPrototypeExtensionAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Don't need to pre-process function-classes -- they are introduced from preprocessing functions.
  if (Name->IsCompilerGeneratedType()) { return; }
  Ast::Stage1_PreProcess(ctx);

  // Preprocess the implementation.
  Impl->Stage1_PreProcess(this);

  // Todo: some sort of check that prevents something like "sup [T] T ext Borrow[T]", because this is infinite generation
}

auto SupPrototypeExtensionAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  using analyse::errors::SppSuperimpositionOptionalGenericParameterError;
  using analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError;

  // Create a new scope for the superimposition extension.
  auto scope_name = ScopeBlockName::FromParts(
    "sup-prototype-extension", {Name.get(), SuperClass.get()}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  // Check there are optional generic parameters.
  const auto optional = GnParamGroup->GetOptionalParams();
  RaiseIf<SppSuperimpositionOptionalGenericParameterError>(
    not optional.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(*optional[0]));

  // Check every generic parameter is constrained by the type.
  if (not Name->IsCompilerGeneratedType()) {
    const auto unconstrained = GnParamGroup->GetAllParams()
      | genex::views::filter([this](auto const &x) {
        return not(Name->ContainsGenerics(*x) or SuperClass->ContainsGenerics(*x));
      })
      | genex::to<Vec>();
    RaiseIf<SppSuperimpositionUnconstrainedGenericParameterError>(
      not unconstrained.IsEmpty(), {sm->CurrentScope},
      ERR_ARGS(*unconstrained[0]));
  }

  // Generate symbols for the generic parameter group, and the self type.
  GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
  Impl->Stage2_GenTopLvlScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Register "Self" before any alias in the block is resolved,
  // so that one naming it has something to resolve to. The name
  // is not qualified yet, so the base symbol is what answers
  // here; Stage 5 replaces this with the precise one.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  if (not Name->IsCompilerGeneratedType()) {
    // The name need not resolve to anything here: a superimposition over a type that does not exist is reported by
    // the stage that qualifies it, not this one, so this asks for the symbol rather than assuming it.
    if (const auto base_sym = sm->CurrentScope->GetTypeSymbol(Name->WithoutGenerics().get())) {
      sm->AddSelfTypeSymbol(base_sym->LinkedScope, Name->PosStart());
    }
  }
  Impl->Stage3_GenTopLvlAliases(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward to the implementation.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  GnParamGroup->Stage4_ResolveDeclarations(sm, meta);
  Impl->Stage4_ResolveDeclarations(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  using analyse::utils::type_predicates::IsTypeBorrowed;
  using analyse::errors::SppGenericTypeInvalidUsageError;
  using analyse::errors::SppSecondClassBorrowViolationError;

  // Move into the superimposition scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Analyse the type being superimposed over. An abstract
  // type is allowed here, because this is where its
  // abstract methods are declared, and where a derived
  // type implements them.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->AllowAbstractType = true;
    Name->Stage7_AnalyseSemantics(sm, meta);
  }

  RaiseIf<SppSecondClassBorrowViolationError>(
    IsTypeBorrowed(*Name, *sm),
    {sm->CurrentScope}, ERR_ARGS(*this, *Name, "superimposition type"));

  // A "$Func" mock keeps its bare name here. This is a
  // declaration site, always in the same module as the
  // mock class it names. Keep it as the original
  // identifier in terms of namespacing.
  Name = sm->CurrentScope->GetTypeSymbol(Name.get())->FqName(true)->WithSourceSpanOf(*Name);

  // Register the superimposition against the base symbol. A
  // method's "$" mock block sits inside its "sup" block rather
  // than the module, and still has to reach its mock class.
  const auto base_cls_sym = sm->CurrentScope->GetTypeSymbol(Name->WithoutGenerics().get());
  if (sm->CurrentScope->Parent == sm->CurrentScope->ParentModule() or Name->IsCompilerGeneratedType()) {
    if (not base_cls_sym->IsTypeGeneric()) {
      ScopeManager::normal_sup_blocks[base_cls_sym].EmplaceBack(sm->CurrentScope);
    }
    else {
      ScopeManager::generic_sup_blocks.EmplaceBack(sm->CurrentScope);
    }
  }

  // Re-register "Self" against the fully-resolved name, replacing
  // the provisional one from Stage 3.
  if (not Name->IsCompilerGeneratedType()) {
    sm->AddSelfTypeSymbol(
      sm->CurrentScope->GetTypeSymbol(Name.get())->LinkedScope, Name->PosStart());
  }

  // Analyse the supertype after Self has been added
  // (allows use in generic arguments to the superclass).
  SuperClass->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppSecondClassBorrowViolationError>(
    IsTypeBorrowed(*SuperClass, *sm),
    {sm->CurrentScope}, ERR_ARGS(*this, *SuperClass, "superimposition supertype"));
  SuperClass = sm->CurrentScope->GetTypeSymbol(SuperClass.get())->FqName()->WithSourceSpanOf(*SuperClass);

  // Check the supertype is not generic.
  const auto sup_sym = sm->CurrentScope->GetTypeSymbol(SuperClass.get());
  RaiseIf<SppGenericTypeInvalidUsageError>(
    sup_sym->IsTypeGeneric(), {sm->CurrentScope},
    ERR_ARGS(*SuperClass, *SuperClass, "superimposition supertype"));

  // Load the implementation and move out of the scope.
  Impl->Stage5_LoadSupScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  using analyse::utils::func_utils::CheckForConflictingOverride;
  using analyse::utils::type_members::CheckShadowedCmpAgreesInType;
  using analyse::utils::type_compare::TypeEq;
  using analyse::errors::SppSuperimpositionExtensionMethodInvalidError;
  using analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError;
  using analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError;
  using analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError;
  using generate::common_types_precompiled::COPY;

  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Re-analyse the superclass type so its generic-argument
  // constraints are enforced at this pre-analysis stage. If
  // they are done in stage 7, then we get misleading errors
  // because when something else correctly fails, a missing
  // constraint enforcement spews some inference error..
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->AllowAbstractType = true;
    SuperClass->ResetCache();
    SuperClass->Stage7_AnalyseSemantics(sm, meta);
  }

  // Get the symbols.
  const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name.get());
  const auto sup_sym = sm->CurrentScope->GetTypeSymbol(SuperClass.get());

  // We use "direct" super scopes here because it enforces
  // superimposing "Copy" on the type directly, not via an
  // extension chain.
  auto sup_scopes = sm->CurrentScope->GetTypeSymbol(SuperClass.get())->LinkedScope->DirectSupScopes;
  sup_scopes |= genex::actions::insert(sup_scopes.begin(), sup_sym->LinkedScope);
  sup_scopes |= genex::actions::remove_if([](auto const &x) {
    return AstAs<ClassPrototypeAst>(x->AstNode) == nullptr;
  });

  // Mark the class as copyable if the "Copy" type is the
  // supertype.
  for (const auto sup_scope : sup_scopes) {
    if (analyse::utils::type_predicates::IsTemplate(*sup_scope->TySym, *COPY, *sup_scope)) {
      sm->CurrentScope->GetTypeSymbol(Name->WithoutGenerics().get())->IsDirectlyCopyable = true;
      cls_sym->IsDirectlyCopyable = true;
      break;
    }
  }

  // Check every member on the superimposition exists on
  // the supertype.
  for (auto const &member : Impl->Members) {
    if (const auto ext_member = member->To<SupPrototypeExtensionAst>()) {
      // Get the method and identify the base method it
      // is overriding.
      const auto this_method = ext_member->Impl->FinalMember()->To<FunctionPrototypeAst>();
      const auto base_method = CheckForConflictingOverride(
        *member->GetAstScope(), sup_sym->LinkedScope, *this_method, *sm, meta);

      // Check the base method exists.
      RaiseIf<SppSuperimpositionExtensionMethodInvalidError>(
        base_method == nullptr, {sm->CurrentScope},
        ERR_ARGS(*this_method->Name, *SuperClass));

      // Check the base method is virtual or abstract.
      RaiseIf<SppSuperimpositionExtensionNonVirtualMethodOverriddenError>(
        not(base_method->AbstractAnnotation or base_method->VirtualAnnotation), {sm->CurrentScope},
        ERR_ARGS(*this_method->Name, *base_method->Name, *SuperClass));

      // Sync up the annotations from the base function.
      // Todo: Once "inheriting" annotations is supported at definition, do it dynamically.
      this_method->Visibility = base_method->Visibility;
      const auto func_sym = sm->CurrentScope->GetVarSymbol(this_method->Name.get(), true);
      func_sym->Visibility = base_method->Visibility.first;
      func_sym->VisibilityAnnotation = this_method->Visibility.second;
    }

    else if (const auto type_member = member->To<TypeStatementAst>()) {
      // Get the associated type from the supertype directly.
      const auto this_type = type_member->NewType;
      const auto base_type = sup_sym->LinkedScope->GetTypeSymbol(
        this_type.get(), true);

      // Check to see if the base type exists.
      RaiseIf<SppSuperimpositionExtensionTypeStatementInvalidError>(
        base_type == nullptr, {sm->CurrentScope, member->GetAstScope()},
        ERR_ARGS(*type_member, *SuperClass));
    }

    else if (const auto cmp_member = member->To<CmpStatementAst>()) {
      // Get the associated cmp from the supertype directly.
      const auto this_const = cmp_member->Name;
      const auto base_const = sup_sym->LinkedScope->GetVarSymbol(
        this_const.get(), true);

      // Check to see if the base cmp exists. Same as method,
      // as part of an extension, it's "overriding" the
      // declaration.
      RaiseIf<SppSuperimpositionExtensionCmpStatementInvalidError>(
        base_const == nullptr, {sm->CurrentScope},
        ERR_ARGS(*cmp_member, *SuperClass));

      // Check the constant agrees in type with every declaration
      // of that name on the type and its super types.
      CheckShadowedCmpAgreesInType(
        *cmp_member, *cls_sym->LinkedScope, *sm->CurrentScope, *sm);
    }
  }

  // Pre-analyse the implementation.
  Impl->Stage6_PreAnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  using analyse::utils::generic_bindings::EnforceGenericConstraintsAllArgs;

  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Re-map "Self" to the true type.
  sm->SyncSelfTypeSymbol(*Name);

  GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

  // Both the superimposition target and the superclass are allowed to be abstract, as neither names a value.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->AllowAbstractType = true;

    Name->ResetCache();
    Name->Stage7_AnalyseSemantics(sm, meta);
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name.get());
    if (cls_sym->Type)
      analyse::utils::generic_bindings::EnforceGenericConstraintsOfParams(*cls_sym, *GnParamGroup, *sm, *meta);

    SuperClass->ResetCache();
    SuperClass->Stage7_AnalyseSemantics(sm, meta);
    if (cls_sym->Type and not cls_sym->IsMock()) {
      const auto sup_sym = sm->CurrentScope->GetTypeSymbol(SuperClass.get());
      analyse::utils::generic_bindings::EnforceGenericConstraintsOfParams(*sup_sym, *GnParamGroup, *sm, *meta);
    }
  }

  Impl->Stage7_AnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage8_CheckMemory(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage9_CompTimeResolve(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage10_PreCodeGen(sm, meta, ctx);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto SupPrototypeExtensionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Move to the next scope.
  sm->MoveToNextScope();
  Impl->Stage11_CodeGen(sm, meta, ctx);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto SupPrototypeExtensionAst::CheckCyclicExtension(
  TypeSymbol const &sup_sym, Scope &check_scope) const -> void {
  // Prevent cyclic inheritance: this block's super class already extending its type, at any level.
  using analyse::errors::SppSuperimpositionCyclicExtensionError;
  const auto cycle = FindMatchingExtension(sup_sym.LinkedScope->SupScopes(), *Name, *SuperClass, check_scope, true);
  RaiseIf<SppSuperimpositionCyclicExtensionError>(
    cycle.second != nullptr, {&check_scope}, ERR_ARGS(*cycle.second->SuperClass, *SuperClass));
}

auto SupPrototypeExtensionAst::CheckDoubleExtension(
  TypeSymbol const &cls_sym, Scope &check_scope) const -> void {
  // Prevent double inheritance: the same type extended by the same super class, at this level. A function class's
  // blocks are generated one per overload, so it is not checked.
  using analyse::errors::SppSuperimpositionDoubleExtensionError;
  if (cls_sym.IsMock()) { return; }
  const auto twin = FindMatchingExtension(cls_sym.LinkedScope->DirectSupScopes, *SuperClass, *Name, check_scope, false);
  if (twin.second != nullptr) {
    Raise<SppSuperimpositionDoubleExtensionError>(
      {twin.first, &check_scope}, ERR_ARGS(*twin.second->SuperClass, *SuperClass));
  }
}

auto SupPrototypeExtensionAst::CheckSelfExtension(
  Scope &check_scope) const -> void {
  //
  using analyse::errors::SppSuperimpositionSelfExtensionError;
  using analyse::utils::type_compare::TypeEq;

  // Optimization as $Types can never extend themselves, given
  // that they are compiler generated.
  // Todo: Apply to cyclic and double extension checks too?
  if (Name->IsCompilerGeneratedType()) { return; }

  // Check if the superimposition is extending itself.
  RaiseIf<SppSuperimpositionSelfExtensionError>(
    TypeEq(*Name, *SuperClass, check_scope, check_scope), {&check_scope},
    ERR_ARGS(*Name, *SuperClass));
}

SPP_MOD_END
