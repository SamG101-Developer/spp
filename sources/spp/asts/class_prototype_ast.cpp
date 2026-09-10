module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.class_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import genex;
import llvm;

SPP_MOD_BEGIN
spp::asts::ClassPrototypeAst::ClassPrototypeAst(
  decltype(Annotations) &&annotations,
  decltype(TokCls) &&tok_cls,
  decltype(Name) name,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(Impl) &&impl) :
  Annotations(std::move(annotations)),
  TokCls(std::move(tok_cls)),
  ZeroTypeAnnotation(nullptr),
  Name(std::move(name)),
  GnParamGroup(std::move(generic_param_group)),
  Impl(std::move(impl)),
  _ClsSym(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCls, lex::SppTokenType::KW_CLS, "cls");
  SPP_SET_AST_TO_DEFAULT_SHARED_IF_NULLPTR(this->GnParamGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
}

spp::asts::ClassPrototypeAst::~ClassPrototypeAst() = default;

auto spp::asts::ClassPrototypeAst::PosStart() const
  -> std::size_t {
  // Use the "cls" token.
  return TokCls->PosStart();
}

auto spp::asts::ClassPrototypeAst::PosEnd() const
  -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto spp::asts::ClassPrototypeAst::Clone() const
  -> Unique<Ast> {
  auto ast = MakeUnique<ClassPrototypeAst>(
    AstCloneVec(Annotations),
    AstClone(TokCls),
    AstClone(Name),
    AstClone(GnParamGroup),
    AstClone(Impl));
  ast->Visibility = Visibility;
  ast->ZeroTypeAnnotation = ZeroTypeAnnotation;
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  ast->_ClsSym = _ClsSym;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto spp::asts::ClassPrototypeAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Annotations, "\n");
  SPP_STRING_APPEND_RAW(not Annotations.IsEmpty() ? "\n" : "");
  SPP_STRING_APPEND(TokCls).append(" ");
  SPP_STRING_APPEND(Name);
  SPP_STRING_APPEND(GnParamGroup).append(" ");
  SPP_STRING_APPEND(Impl);
  SPP_STRING_END;
}

auto spp::asts::ClassPrototypeAst::Stage1_PreProcess(
  Ast *ctx)
  -> void {
  // Pre-process the AST by calling the base class method and then processing annotations and the body.
  Ast::Stage1_PreProcess(ctx);
  for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
  Impl->Stage1_PreProcess(this);
}

auto spp::asts::ClassPrototypeAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Create the class scope, which is the scope for the class prototype.
  auto scope_name = analyse::scopes::ScopeTypeIdentifierName(Name);
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  // Run the generation steps for the annotations.
  for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

  // Generate the symbols for the class prototype, and handle generic parameters.
  meta->ClsSym = _GenerateSymbols(sm);
  GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
  Impl->Stage2_GenTopLvlScopes(sm, meta);

  // Move out of the class scope, as the class scope is now
  // complete.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm,
  CompilerMetaData *)
  -> void {
  // Register "Self" before any alias in the body is resolved,
  // so that one naming it has something to resolve to. A class's
  // "Self" is its own scope, which is the scope just moved into.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  if (not Name->IsCompilerGeneratedType()) {
    sm->AddSelfTypeSymbol(sm->CurrentScope, Name->PosStart());
  }
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage4_QualifyTypes(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Qualify the types in the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage4_QualifyTypes(sm, meta); }
  GnParamGroup->Stage4_QualifyTypes(sm, meta);
  Impl->Stage4_QualifyTypes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage5_LoadSupScopes(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Load the super scopes for the class body.
  using analyse::utils::type_compare::TypeEq;
  using generate::common_types_precompiled::COPY;
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }

  // Sync the type symbols' visibility from the AST.
  if (_ClsSym != nullptr) { _ClsSym->Visibility = Visibility.first; }
  if (sm->CurrentScope->TySym != nullptr) {
    sm->CurrentScope->TySym->Visibility = Visibility.first;
  }

  // Visibility patch for generic symbols ie Vec vs
  // Vec[T]. Sync the visibility.
  if (not GnParamGroup->Params.IsEmpty() and sm->CurrentScope->Parent != nullptr) {
    const auto base_sym = sm->CurrentScope->Parent->GetTypeSymbol(Name->TypeParts()[0], true);
    if (base_sym != nullptr) { base_sym->Visibility = Visibility.first; }
  }

  // Mark the "Copy" class itself as copyable. Minimise
  // `TypeEq` calls.
  if (_ClsSym != nullptr and Name->LastTypePart()->Name == COPY->LastTypePart()->Name) {
    const auto fq_name = _ClsSym->FqName();
    if (TypeEq(*fq_name, *COPY, *sm->CurrentScope, *sm->CurrentScope)) {
      sm->CurrentScope->GetTypeSymbol(Name->WithoutGenerics().get())->IsDirectlyCopyable = true;
      _ClsSym->IsDirectlyCopyable = true;
    }
  }

  // Re-register "Self" now that the name resolves precisely.
  // Stage 3 registered a provisional one so that the body's
  // aliases could name it; this replaces it with the scope
  // the fully-resolved name links to.
  if (not Name->IsCompilerGeneratedType()) {
    sm->AddSelfTypeSymbol(
      sm->CurrentScope->GetTypeSymbol(Name.get())->LinkedScope, Name->PosStart());
  }

  Impl->Stage5_LoadSupScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Pre-analyse semantics for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage6_PreAnalyseSemantics(sm, meta);

  // Check the type isn't recursive.
  const auto recursion = analyse::utils::type_predicates::IsTypeRecursive(*this, *sm);
  RaiseIf<analyse::errors::SppRecursiveTypeError>(
    recursion != nullptr, {sm->CurrentScope},
    ERR_ARGS(*this, *recursion));

  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Analyse semantics for the class body.
  using generate::common_types_precompiled::SELF_TYPE;
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Re-map "Self" to the true type.
  if (not Name->IsCompilerGeneratedType()) {
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name.get());
    const auto self_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get(), true);
    self_sym->Type = cls_sym->Type;
    cls_sym->AliasedBySyms.EmplaceBack(self_sym->SharedFromThis<analyse::scopes::TypeSymbol>());
  }

  for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }
  GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

  // A "!zero_type" class is guaranteed to occupy no storage - every use of one assumes as much - so it cannot declare
  // state of its own. Being zero-sized says nothing about copying: a marker is still linear unless it is "Copy".
  RaiseIf<analyse::errors::SppEmptyBodyRequiredError>(
    ZeroTypeAnnotation != nullptr and not Impl->Members.IsEmpty(),
    {sm->CurrentScope}, ERR_ARGS(
      *ZeroTypeAnnotation, *Impl->Members.Front(), "a '!zero_type' class",
      "the type is guaranteed to occupy no storage, and an attribute would give it a size"));

  Impl->Stage7_AnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Check memory for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage8_CheckMemory(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Skip the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
  GnParamGroup->Stage9_CompTimeResolve(sm, meta);
  Impl->Stage9_CompTimeResolve(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage10_PreCodeGen(
  ScopeManager *sm,
  CompilerMetaData *,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Generate code for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  const auto cls_sym = sm->CurrentScope->TySym;

  // $ types are pre-set with a packed empty body.
  if (Name->IsCompilerGeneratedType()) {
    sm->MoveOutOfCurrentScope();
    return nullptr;
  }

  // If this is a raw generic class like Vec[T], then generate the generic implementations.
  if (genex::any_of(sm->CurrentScope->AllTypeSymbols(), [](auto const &sym) { return sym->IsGeneric; })) {
    for (auto const &[generic_scope, generic_ast] : _GenericSubstitutions) {
      generic_ast->FillLlvmLayout(sm, generic_scope->TySym.get(), ctx);
    }
  }

  FillLlvmLayout(sm, cls_sym.get(), ctx);

  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto spp::asts::ClassPrototypeAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Get the class symbol.
  sm->MoveToNextScope();
  // SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage11_CodeGen(sm, meta, ctx);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto spp::asts::ClassPrototypeAst::RegisterGenericSubstitution(
  analyse::scopes::Scope *scope,
  Unique<ClassPrototypeAst> &&new_ast)
  -> void {
  // Just somewhere to store the new_ast as a unique_ptr.
  _GenericSubstitutions.EmplaceBack(scope, std::move(new_ast));
}

auto spp::asts::ClassPrototypeAst::GetRegisteredGenericSubstitutions() const
  -> Vec<Pair<analyse::scopes::Scope*, ClassPrototypeAst*>> {
  // Return the generic substituted scopes as raw pointers.
  return _GenericSubstitutions
    | genex::views::transform([](auto const &x) { return MakePair(x.first, x.second.get()); })
    | genex::to<Vec>();
}

auto spp::asts::ClassPrototypeAst::GetClsSym() const
  -> Shared<analyse::scopes::TypeSymbol> {
  return _ClsSym;
}

auto spp::asts::ClassPrototypeAst::_GenerateSymbols(
  ScopeManager *sm)
  -> analyse::scopes::TypeSymbol* {
  auto is_dollar_type = Name->IsCompilerGeneratedType();
  auto sym_name = AstClone(Name->TypeParts()[0]);
  sym_name->GnArgGroup = GenericArgumentGroupAst::FromParams(*GnParamGroup);

  // Create the symbols as TypeSymbol pointers, so AliasSymbols can also be used.
  Shared<analyse::scopes::TypeSymbol> symbol_1 = nullptr;
  Shared<analyse::scopes::TypeSymbol> symbol_2 = nullptr;

  // Create the symbol for the type, include generics if applicable, like Vec[T].
  symbol_1 = MakeShared<analyse::scopes::TypeSymbol>(
    std::move(sym_name), this, sm->CurrentScope, sm->CurrentScope, sm->CurrentScope->ParentModule(), false,
    is_dollar_type);
  sm->CurrentScope->TySym = symbol_1;
  sm->CurrentScope->Parent->AddTypeSymbolCheckConflict(symbol_1);
  _ClsSym = sm->CurrentScope->TySym;

  // A class that still declares parameters is a template, and a template has no layout: its attributes are written in
  // terms of names that stand for nothing yet, so there is no size to give and nothing that can be built against it.
  // Only its instantiations are real types. Its own symbol names it as "Vec[T=T]", which reads like an instantiation
  // and is why this has to be said outright.
  symbol_1->IsConcrete = GnParamGroup->Params.IsEmpty();

  // If the type was generic, like Vec[T], also create a base Vec symbol.
  if (not GnParamGroup->Params.IsEmpty()) {
    symbol_2 = MakeShared<analyse::scopes::TypeSymbol>(
      AstClone(Name->TypeParts()[0]), this, sm->CurrentScope, sm->CurrentScope,
      sm->CurrentScope->ParentModule(), false, is_dollar_type);
    symbol_2->GenericImpl = symbol_1.get();
    const auto ret_sym = symbol_2.get();
    sm->CurrentScope->Parent->AddTypeSymbolCheckConflict(symbol_2);
    return ret_sym;
  }

  return _ClsSym.get();
}

static auto ApplyStructLayout(
  llvm::StructType *struct_type,
  spp::Vec<llvm::Type*> const &field_types,
  const spp::codegen::StructLayout layout,
  spp::codegen::LlvmTypeSymInfo *sym_info,
  spp::codegen::LlvmCtx const *ctx)
  -> void {
  // A struct body is only ever set once. "RegisterLlvmTypeInfo"
  // lays the compiler-known types out itself, like "Var" is a
  // { tag, payload } pair, "Generated" and the "Fun*" family
  // are { fn_ptr, env_ptr } literals. As none of them declare
  // attributes, we need to skip setting 0 fields, as this messes
  // up the layout and subsequent GEP instructions.
  const auto needs_body = struct_type->isOpaque();

  // Fields that carry no value are not laid out at all, whichever convention is in force.
  const auto kept = spp::codegen::DropValuelessFields(field_types);
  auto kept_types = spp::Vec<llvm::Type*>();
  auto kept_map = spp::Map<std::size_t, std::size_t>();
  for (auto new_idx = 0uz; new_idx < kept.Len(); ++new_idx) {
    kept_types.EmplaceBack(field_types[kept[new_idx]]);
    kept_map[kept[new_idx]] = new_idx;
  }
  const auto dropped_any = kept.Len() != field_types.Len();

  switch (layout) {
    case spp::codegen::StructLayout::C: {
      // Keep declaration order, with natural alignment padding.
      // This mirrors the C language / specifications. Seen in the
      // FFI structs.
      if (needs_body) { struct_type->setBody(kept_types.ToStdVector(), false); }
      // An empty map means the declaration order was preserved outright, which it only is when nothing was dropped.
      if (dropped_any) { sym_info->FieldIndexMap = std::move(kept_map); }
      else { sym_info->FieldIndexMap.clear(); }
      break;
    }
    case spp::codegen::StructLayout::Packed: {
      // Keep declaration order, but remove all inter-field padding.
      if (needs_body) { struct_type->setBody(kept_types.ToStdVector(), true); }
      if (dropped_any) { sym_info->FieldIndexMap = std::move(kept_map); }
      else { sym_info->FieldIndexMap.clear(); }
      break;
    }
    case spp::codegen::StructLayout::Spp: {
      // Re-order the fields to minimize padding, and record where
      // each declared attribute ended up, so that codegen can map
      // a declaration index to its physical field index.
      auto [sorted_types, index_map] = spp::codegen::SortMembersForSppLayout(field_types, ctx);
      if (needs_body) { struct_type->setBody(sorted_types.ToStdVector(), false); }
      sym_info->FieldIndexMap = std::move(index_map);
      break;
    }
    default: {
      std::unreachable();
    }
  }
}

auto spp::asts::ClassPrototypeAst::FillLlvmLayout(
  ScopeManager const *sm,
  analyse::scopes::TypeSymbol const *type_sym,
  codegen::LlvmCtx const *ctx) const
  -> void {
  // Todo: error if attribute's default value if a comp generic value?? Also TEST THIS
  using analyse::utils::type_predicates::IsTypeTup;
  using analyse::utils::type_members::GetAllAttrs;
  using analyse::utils::type_predicates::GetSuperimposedFatPointerFieldCount;

  // Non-struct types are compiler known special types, so
  // don't have any field generation. Things like numbers,
  // booleans, functions etc.
  const auto lt = codegen::GetLlvmType(*type_sym, ctx);
  if (lt == nullptr or not llvm::isa<llvm::StructType>(lt)) {
    return;
  }

  // A template wearing an instantiation's clothes has no layout to give: "Pass[T=T]" would take a field of its own
  // type and build a cyclic llvm type, which nothing diagnoses - it simply recurses inside "DataLayout" until the
  // stack runs out. Left opaque, it is skipped by everything downstream, exactly as the template it stands for is.
  if (not type_sym->IsConcrete) { return; }

  // Next we need to handle tuples (anonymous index-attribute
  // based classes) vs standard struct classes.
  const auto is_tuple = IsTypeTup(
    *type_sym->FqName(), *sm->CurrentScope);
  auto types = Vec<llvm::Type*>();

  // The "Spp" layout sorts the fields by size and alignment, so
  // every field has to be lowered all the way before any of them
  // can be placed - a field still sitting as an opaque placeholder
  // has no size to sort on. "GetLlvmType" does that on demand, so
  // the order fields are reached in does not matter.
  const auto lower_field = [&](analyse::scopes::TypeSymbol const *field_type_sym) -> llvm::Type* {
    return field_type_sym != nullptr ? codegen::GetLlvmType(*field_type_sym, ctx) : nullptr;
  };

  // Tuple fields are positional based off of the types found
  // in the generic arguments.
  if (is_tuple) {
    const auto elems = type_sym->FqName()->LastTypePart()->GnArgGroup->GetTypeArgs();
    types = elems
      | genex::views::transform([&](auto const &elem) { return sm->CurrentScope->GetTypeSymbol(elem->Val.get()); })
      | genex::views::transform([&](auto const &type) { return lower_field(type); })
      | genex::to<Vec>();
  }

  // Class attributes are read from the attribute types.
  else {
    types = GetAllAttrs(*type_sym->FqName(), *sm->CurrentScope)
      | genex::views::transform([&](auto const &pair) { return spp::get<1>(pair); })
      | genex::views::transform([&](auto const &type) { return lower_field(type); })
      | genex::to<Vec>();
  }

  // A class that superimposes one of the "Fun*"/"Gen*" family (eg
  // "Iterator[T]" over "Gen[T]") shares its exact runtime shape too.
  // The fat pointer's fields go ahead of whatever fields this class
  // declares of its own.
  const auto fat_pointer_field_count = GetSuperimposedFatPointerFieldCount(
    *type_sym->FqName(), *sm->CurrentScope);
  if (fat_pointer_field_count > 0) {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    auto prefixed = Vec<llvm::Type*>(fat_pointer_field_count, ptr_ty);
    prefixed.AppendRange(types);
    types = std::move(prefixed);
  }

  // If there are any generic types present (llvm_type is nullptr), skip
  // the layout generation. Tuples use "C" layout so they keep the order
  // of types based on the type arguments (standard tuple design).
  if (genex::all_of(types, [](auto const &x) { return x != nullptr; })) {
    const auto struct_type = llvm::dyn_cast<llvm::StructType>(lt);
    const auto layout = is_tuple ? codegen::StructLayout::C : codegen::StructLayout::Spp;
    ApplyStructLayout(struct_type, types, layout, type_sym->LlvmInfo.get(), ctx);
  }

  // Pass this layout to aliases too (the field re-ordering as well as
  // the type itself).
  for (auto const &alias : type_sym->AliasedBySyms) {
    alias->LlvmInfo->LlvmType = lt;
    alias->LlvmInfo->FieldIndexMap = type_sym->LlvmInfo->FieldIndexMap;
  }
}

SPP_MOD_END
