module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.sup_prototype_extension_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.fn_signatures;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.self_type;
import spp.analyse.utils.sup_blocks;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
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
import spp.utils.files;
import spp.utils.ice;
import genex;
import std;

namespace spp::asts {
  namespace {
    /// The package a scope belongs to. This is the library folder
    /// that is a direct child of the "vcs" folder, or "" for the
    /// project's modules.
    auto PackageOfScope(Scope const *const scope) -> Opt<Str> {
      const auto mod_scope = scope != nullptr ? scope->GetParentModule() : nullptr;
      const auto mod_ast = mod_scope != nullptr ? AstAs<ModulePrototypeAst>(mod_scope->AstNode) : nullptr;
      if (mod_ast == nullptr) { return Nullopt; }

      auto const &path = mod_ast->FilePath;
      for (auto it = path.begin(); it != path.end(); ++it) {
        if (*it != "vcs") { continue; }
        if (const auto lib = std::next(it); lib != path.end()) { return spp::utils::files::NativeString(*lib); }
      }
      return Str();
    }

    /// The first block in "scopes" extending a type matching
    /// "name" with a super class matching "super_class". The
    /// super class is compared first: it rejects the large
    /// majority of candidates for a fraction of the cost of a
    /// name comparison, which is the type-symbol lookup this
    /// compiler spends most of its time on.
    auto FindMatchingExtension(
      Vec<Scope*> const &scopes,
      TypeAst const &super_class,
      TypeAst const &name,
      Scope const &check_scope,
      const bool check_constraints)
      -> Pair<Scope*, SupPrototypeExtensionAst const*> {
      IMPORT_UTILS;
      // By identity, read where each is written; nothing is made while the blocks are searched. Either pattern may
      // take the other.
      const auto super_id = check_scope.TypeIdOf(super_class);
      const auto name_id = check_scope.TypeIdOf(name);
      if (super_id == nullptr or name_id == nullptr) { return {nullptr, nullptr}; }
      for (auto *const sc : scopes) {
        const auto ext = AstAs<SupPrototypeExtensionAst>(sc->AstNode);
        if (ext == nullptr or sc->TypeIdOf(*ext->SuperCls) != super_id) { continue; }
        const auto ext_id = sc->TypeIdOf(*ext->Name);
        if (ext_id == nullptr) { continue; }
        auto fwd = analyse::scopes::GenericSubst();
        auto rev = analyse::scopes::GenericSubst();
        if (analyse::scopes::UnifyTypeIds(ext_id, name_id, *sc, check_scope, fwd, false, check_constraints)
          or analyse::scopes::UnifyTypeIds(name_id, ext_id, check_scope, *sc, rev, false, check_constraints)) {
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
  decltype(SuperCls) super_class,
  decltype(Impl) &&impl) :
  TokSup(std::move(tok_sup)),
  GnParamGroup(std::move(generic_param_group)),
  Name(std::move(name)),
  TokExt(std::move(tok_ext)),
  SuperCls(std::move(super_class)),
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
  return SuperCls->PosEnd();
}

auto SupPrototypeExtensionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<SupPrototypeExtensionAst>(
    AstClone(TokSup),
    AstClone(GnParamGroup),
    AstClone(Name),
    AstClone(TokExt),
    AstClone(SuperCls),
    AstClone(Impl));
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  return ast;
}

auto SupPrototypeExtensionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokSup).append(" ");
  SPP_STRING_APPEND(GnParamGroup).append(GnParamGroup->Params.empty() ? "" : " ");
  SPP_STRING_APPEND(Name).append(" ");
  SPP_STRING_APPEND(TokExt).append(" ");
  SPP_STRING_APPEND(SuperCls).append(" ");
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
}

auto SupPrototypeExtensionAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Create a new scope for the superimposition extension.
  auto scope_name = ScopeBlockName::FromParts(
    "sup-prototype-extension", {Name.get(), SuperCls.get()}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  // The generic parameters' symbols first: whether each is named in the type or the supertype is read by identity. A
  // "$" mock block's parameters are the function's own, which its type need not name.
  GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
  sup_blocks::CheckGnParams(
    *GnParamGroup,
    Name->IsCompilerGeneratedType() ? Vec<TypeAst const*>() : Vec<TypeAst const*>{Name.get(), SuperCls.get()}, *sm);

  Impl->Stage2_GenTopLvlScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  self_type::RegisterProvisionalSelf(*Name, *sm);
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
  IMPORT_UTILS;

  // Move into the superimposition scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // The type superimposed over: analysed, qualified, the block filed against it (a method's "$" mock block too), and
  // "Self" made precise.
  const auto base_cls_sym = sup_blocks::LoadTarget(*this, Name, true, *sm, meta);

  // A method's "$" mock block sits in its "sup" block and has no
  // "Self" of its own, so the function type's "Self" is that
  // block's owner: read in by identity, before it is analysed.
  if (Name->IsCompilerGeneratedType() and sm->CurrentScope->Parent != sm->CurrentScope->GetParentModule()) {
    SuperCls = self_type::SubstituteSelf(
      *SuperCls, sm->CurrentScope->FindEnclosingSelfType(*meta).get(), *sm->CurrentScope);
  }

  // Analyse the supertype after Self has been added
  // (allows use in generic arguments to the superclass).
  SuperCls->Stage7_AnalyseSemantics(sm, meta);
  common_errors::RequireNotBorrowed(
    *SuperCls, *this, "superimposition supertype", *sm);

  // Check the supertype is not generic.
  const auto sup_sym = sm->CurrentScope->FindTypeSymbol(SuperCls.get());
  RaiseIf<SppGenericTypeInvalidUsageError>(
    sup_sym->IsGn(),
    ERR_ARGS(*SuperCls, *SuperCls, "superimposition supertype"));

  // A marker is restricted to the package declaring the type:
  // "Copy" and "Drop" decide how every value of that type is
  // handled, and "FwdRef" / "FwdMut" which methods a call on it
  // finds, so one superimposed from outside would silently
  // change the analysis of code that cannot see it - including
  // the package's own, which is analysed as if it were absent.
  if (not Name->IsCompilerGeneratedType()) {
    const auto super_kind = TypeRef::ForKindCheck(*SuperCls, *sm->CurrentScope);
    auto const &scope = *sm->CurrentScope;
    const auto is_marker = marker_sups::IsCopyMarker(super_kind, scope)
      or marker_sups::IsDropMarker(super_kind, scope)
      or marker_sups::IsFwdMarker(super_kind, scope);

    // A blanket "sup [T] T ext Copy" names a generic parameter
    // rather than a type, so no package declares what it marks,
    // and it marks every type there is.
    auto owner_package = Opt<Str>();
    if (is_marker and base_cls_sym->Kind != TypeKind::GnTypeParam) {
      owner_package = PackageOfScope(base_cls_sym->LinkedScope);
    }

    RaiseIf<SppSuperimpositionExternalMarkerExtensionError>(
      is_marker and owner_package != PackageOfScope(sm->CurrentScope),
      ERR_ARGS(*Name, *SuperCls));
  }

  // Load the implementation and move out of the scope.
  Impl->Stage5_LoadSupScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

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
    SuperCls->ResetCache();
    SuperCls->Stage7_AnalyseSemantics(sm, meta);
  }

  // Get the symbols.
  const auto cls_sym = sm->CurrentScope->FindTypeSymbol(Name.get());
  const auto sup_sym = sm->CurrentScope->FindTypeSymbol(SuperCls.get());

  // We use "direct" super scopes here because it enforces
  // superimposing "Copy" on the type directly, not via an
  // extension chain.
  auto sup_scopes = sm->CurrentScope->FindTypeSymbol(SuperCls.get())->LinkedScope->DirectSupScopes;
  sup_scopes |= genex::actions::insert(sup_scopes.begin(), sup_sym->LinkedScope);
  sup_scopes |= genex::actions::remove_if([](auto const &x) {
    return AstAs<ClassPrototypeAst>(x->AstNode) == nullptr;
  });

  // Mark the class as copyable if the "Copy" type is the supertype (that its attributes are all copyable is checked
  // in stage 7).
  for (const auto sup_scope : sup_scopes) {
    if (marker_sups::IsCopyMarker(TypeRef::ForKindCheck(*sup_scope), *sup_scope)) {
      sm->CurrentScope->FindHeadSymbol(*Name)->IsDirectlyCopyable = true;
      cls_sym->IsDirectlyCopyable = true;
      break;
    }
  }

  // Pre-analyse the implementation.
  Impl->Stage6_PreAnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeExtensionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  //

  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

  // Both the superimposition target and the superclass are allowed to be abstract, as neither names a value.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->AllowAbstractType = true;

    Name->ResetCache();
    Name->Stage7_AnalyseSemantics(sm, meta);
    const auto cls_sym = sm->CurrentScope->FindTypeSymbol(Name.get());

    SuperCls->ResetCache();
    SuperCls->Stage7_AnalyseSemantics(sm, meta);
    if (cls_sym->Type and not cls_sym->IsMock()) {
      const auto sup_sym = sm->CurrentScope->FindTypeSymbol(SuperCls.get());

      // Copying a value copies every attribute, so "Copy" only
      // holds over a type whose attributes are all copyable -
      // otherwise the copy duplicates something owned (a "Str"
      // buffer), which is then destroyed twice. An attribute of
      // a generic parameter's type is left to the instantiation.
      if (marker_sups::IsCopyMarker(TypeRef::ForKindCheck(*sup_sym, *sm->CurrentScope), *sm->CurrentScope)
        and cls_sym->LinkedScope != nullptr) {
        for (auto const *attr : type_members::GetAllAttrAsts(*cls_sym)) {
          const auto attr_sym = cls_sym->LinkedScope->FindTypeSymbol(attr->Type.get());
          RaiseIf<SppGenericConstraintError>(
            attr_sym != nullptr and not attr_sym->IsGn() and not attr_sym->IsCopyable(),
            ERR_ARGS(*SuperCls, *attr->Type));
        }
      }
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

auto SupPrototypeExtensionAst::CheckExtensionMembers(
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  // Checked for every block before any is pre-analysed: a member that overrides nothing leaves the abstract method it
  // meant to implement unimplemented, so a type using this block is abstract, and whichever module happened to name
  // that type first would otherwise report it as an abstract type use instead of this, the cause.
  IMPORT_UTILS;
  const auto cls_sym = _Scope->FindTypeSymbol(Name.get());
  const auto sup_sym = _Scope->FindTypeSymbol(SuperCls.get());

  // Check every member on the superimposition exists on
  // the supertype.
  for (auto const &member : Impl->Members) {
    if (const auto ext_member = member->To<SupPrototypeExtensionAst>()) {
      // Get the method and identify the base method it
      // is overriding.
      const auto this_method = ext_member->Impl->FinalMember()->To<FunctionPrototypeAst>();
      const auto base_method = fn_signatures::CheckForConflictingOverride(
        *member->GetAstScope(), sup_sym->LinkedScope, *this_method, sm, meta);

      // Check the base method exists.
      RaiseIf<SppSuperimpositionExtensionMethodInvalidError>(
        base_method == nullptr,
        ERR_ARGS(*this_method->Name, *SuperCls));

      // Check the base method is virtual or abstract.
      // The base method is shown from the super class, which can be in another file than this block.
      RaiseIf<SppSuperimpositionExtensionNonVirtualMethodOverriddenError>(
        not(base_method->AbstractAnnotation or base_method->VirtualAnnotation),
        ERR_ARGS(*this_method->Name, *base_method->Name, *SuperCls));

      // Sync up the annotations from the base function.
      // Todo: Once "inheriting" annotations is supported at definition, do it dynamically.
      this_method->Visibility = base_method->Visibility;
      const auto func_sym = _Scope->FindVarSymbol(this_method->Name.get(), true);
      func_sym->Visibility = base_method->Visibility.first;
      func_sym->VisibilityAnnotation = this_method->Visibility.second;
    }

    else if (const auto type_member = member->To<TypeStatementAst>()) {
      // Get the associated type from the supertype directly.
      const auto this_type = type_member->NewType;
      const auto base_type = sup_sym->LinkedScope->FindTypeSymbol(
        this_type.get(), true);

      // Check to see if the base type exists.
      RaiseIf<SppSuperimpositionExtensionTypeStatementInvalidError>(
        base_type == nullptr,
        ERR_ARGS(*type_member, *SuperCls));
    }

    else if (const auto cmp_member = member->To<CmpStatementAst>()) {
      // Get the associated cmp from the supertype directly.
      const auto this_const = cmp_member->Name;
      const auto base_const = sup_sym->LinkedScope->FindVarSymbol(
        this_const.get(), true);

      // Check to see if the base cmp exists. Same as method,
      // as part of an extension, it's "overriding" the
      // declaration.
      RaiseIf<SppSuperimpositionExtensionCmpStatementInvalidError>(
        base_const == nullptr,
        ERR_ARGS(*cmp_member, *SuperCls));

      // Check the constant agrees in type with every declaration
      // of that name on the type and its super types.
      type_members::CheckShadowedCmpAgreesInType(
        *cmp_member, *cls_sym->LinkedScope, *_Scope);
    }
  }
}

auto SupPrototypeExtensionAst::CheckExtensionHierarchy(
  TypeSymbol const &cls_sym, TypeSymbol const *sup_sym, Scope &check_scope) const -> void {
  //
  IMPORT_UTILS;

  // Cyclic: the super class already extends this type, at any
  // level. The extension closing the cycle is shown from its own
  // block, which can be in another file than this one. A super
  // class that did not resolve under this instance's arguments
  // ("Try[.., Value=T]" with "T" bound to "Self", still open) has
  // nothing to be cyclic with.
  if (sup_sym != nullptr and sup_sym->LinkedScope != nullptr) {
    const auto cycle = FindMatchingExtension(sup_sym->LinkedScope->GetSupScopes(), *Name, *SuperCls, check_scope, true);
    RaiseIf<SppSuperimpositionCyclicExtensionError>(
      cycle.second != nullptr,
      ERR_ARGS(*cycle.second->SuperCls, *SuperCls));
  }

  // Double: the same type extended by the same super class, at
  // this level.
  const auto twin = FindMatchingExtension(cls_sym.LinkedScope->DirectSupScopes, *SuperCls, *Name, check_scope, false);
  if (twin.second != nullptr) {
    Raise<SppSuperimpositionDoubleExtensionError>(ERR_ARGS(*twin.second->SuperCls, *SuperCls));
  }

  // Self: the same identity, read where both are written.
  const auto name_id = check_scope.TypeIdOf(*Name);
  RaiseIf<SppSuperimpositionSelfExtensionError>(
    name_id != nullptr and name_id == check_scope.TypeIdOf(*SuperCls),
    ERR_ARGS(*Name, *SuperCls));
}

SPP_MOD_END
