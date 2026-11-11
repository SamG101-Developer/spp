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
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_abi;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.strings;
import genex;
import llvm;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeIdentifierName);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, ScopeTypeIdentifierName);
use_alias(spp::analyse::scopes, TypeKind);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

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
  FfiAnnotation(nullptr),
  ConvAnnotation(nullptr),
  Name(std::move(name)),
  GnParamGroup(std::move(generic_param_group)),
  Impl(std::move(impl)),
  _ClsSymbol(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCls, lex::SppTokenType::KW_CLS, "cls");
  SPP_SET_AST_TO_DEFAULT_SHARED_IF_NULLPTR(this->GnParamGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
}

spp::asts::ClassPrototypeAst::~ClassPrototypeAst() = default;

auto spp::asts::ClassPrototypeAst::PosStart() const -> std::size_t {
  // Use the "cls" token.
  return TokCls->PosStart();
}

auto spp::asts::ClassPrototypeAst::PosEnd() const -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto spp::asts::ClassPrototypeAst::Clone() const -> Unique<Ast> {
  auto ast = MakeUnique<ClassPrototypeAst>(
    AstCloneVec(Annotations),
    AstClone(TokCls),
    AstClone(Name),
    AstClone(GnParamGroup),
    AstClone(Impl));
  ast->Visibility = Visibility;
  ast->ZeroTypeAnnotation = ZeroTypeAnnotation;
  ast->FfiAnnotation = FfiAnnotation;
  ast->ConvAnnotation = ConvAnnotation;
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  ast->_ClsSymbol = _ClsSymbol;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto spp::asts::ClassPrototypeAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Annotations, "\n");
  SPP_STRING_APPEND_RAW(not Annotations.empty() ? "\n" : "");
  SPP_STRING_APPEND(TokCls).append(" ");
  SPP_STRING_APPEND(Name);
  SPP_STRING_APPEND(GnParamGroup).append(" ");
  SPP_STRING_APPEND(Impl);
  SPP_STRING_END;
}

auto spp::asts::ClassPrototypeAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Pre-process the AST by calling the base class method and
  // then processing annotations and the body.
  Ast::Stage1_PreProcess(ctx);
  for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
  Impl->Stage1_PreProcess(this);
}

auto spp::asts::ClassPrototypeAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Create the class scope, which is the scope for the class
  // prototype.
  auto scope_name = ScopeTypeIdentifierName(Name);
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  // Run the generation steps for the annotations.
  for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

  // Generate the symbols for the class prototype, and handle
  // generic parameters.
  _GenerateSymbols(sm);
  GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
  Impl->Stage2_GenTopLvlScopes(sm, meta);

  // Move out of the class scope, as the class scope is now
  // complete.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *) -> void {
  IMPORT_UTILS;
  // Register "Self" before any alias in the body is resolved,
  // so that one naming it has something to resolve to. A class's
  // "Self" is its own scope, which is the scope just moved into.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  if (not Name->IsCompilerGeneratedType()) {
    self_type::AddSelfTypeSymbol(*sm->CurrentScope, sm->CurrentScope, Name->PosStart());
  }
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Qualify the types in the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage4_ResolveDeclarations(sm, meta); }
  GnParamGroup->Stage4_ResolveDeclarations(sm, meta);
  Impl->Stage4_ResolveDeclarations(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  using generate::common_types_precompiled::COPY;

  // Load the super scopes for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }


  // Mark the "Copy" class itself as copyable. Minimise
  // `TypeEq` calls.
  if (_ClsSymbol != nullptr and Name->LastTypePart()->Name == COPY->LastTypePart()->Name) {
    if (marker_sups::IsCopyMarker(TypeRef::ForKindCheck(*_ClsSymbol, *sm->CurrentScope), *sm->CurrentScope)) {
      sm->CurrentScope->FindHeadSymbol(*Name)->IsDirectlyCopyable = true;
      _ClsSymbol->IsDirectlyCopyable = true;
    }
  }

  // A named function's mock carries nothing at runtime - the
  // type alone says which function it is - so it is free to
  // copy, and a call through a value of it does not consume
  // the value.
  if (_ClsSymbol != nullptr and Name->IsCompilerGeneratedType()) {
    _ClsSymbol->IsDirectlyCopyable = true;
  }

  // Re-register "Self" now that the name resolves precisely.
  // Stage 3 registered a provisional one so that the body's
  // aliases could name it; this replaces it with the scope
  // the fully-resolved name links to.
  self_type::RegisterSelf(*Name, *sm);

  Impl->Stage5_LoadSupScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Pre-analyse semantics for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage6_PreAnalyseSemantics(sm, meta);

  // Check the type isn't recursive.
  const auto recursion = type_members::FindRecursiveAttrType(*this, *sm);
  RaiseIf<SppRecursiveTypeError>(
    recursion != nullptr,
    ERR_ARGS(*this, *recursion));

  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Analyse semantics for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }
  GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

  // A "!zero_type" class is guaranteed to occupy no storage -
  // every use of one assumes as much - so it cannot declare
  // state of its own. Being zero-sized says nothing about
  // copying: a marker is still linear unless it is "Copy".
  RaiseIf<SppEmptyBodyRequiredError>(
    ZeroTypeAnnotation != nullptr and not Impl->Members.empty(),
    ERR_ARGS(
      *ZeroTypeAnnotation, *Impl->Members.front(), "a '!zero_type' class",
      "the type is guaranteed to occupy no storage, and an attribute would give it a size"));

  // An "!ffi" class mirrors a C struct, so a "!conv" on it has to
  // name a foreign convention ("C" itself, or a platform's).
  RaiseIf<SppIdentifierUnknownError>(
    FfiAnnotation != nullptr and ConvAnnotation != nullptr and not codegen::AbiOf(*_ClsSymbol).IsForeign(),
    ERR_ARGS(*ConvAnnotation, "convention for an '!ffi' class", spp::utils::strings::ClosestMatch(ConvAnnotation->ConvName(), codegen::AbiNames(true))));

  Impl->Stage7_AnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check memory for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage8_CheckMemory(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate code for the class body.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  const auto cls_sym = sm->CurrentScope->LinkedTypeSymbol;

  // $ types are pre-set with a packed empty body.
  if (Name->IsCompilerGeneratedType()) {
    sm->MoveOutOfCurrentScope();
    return nullptr;
  }

  // If this is a raw generic class like Vec[T], then generate
  // the generic implementations.
  if (genex::any_of(sm->CurrentScope->GetAllTypeSymbols(), [](auto const &sym) { return sym->IsGn(); })) {
    for (auto const &sub : _GnSubstitutions) {
      sub.Proto->FillLlvmLayout(sm, sub.InstanceScope->LinkedTypeSymbol.get(), ctx);
    }
  }

  FillLlvmLayout(sm, cls_sym.get(), ctx);

  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto spp::asts::ClassPrototypeAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Get the class symbol.
  sm->MoveToNextScope();
  // SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage11_CodeGen(sm, meta, ctx);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto spp::asts::ClassPrototypeAst::AddGnSubstitution(
  GenericSubstitution &&sub) -> GenericSubstitution& {
  // Kept alive here: the instantiation's scope points into it.
  return _GnSubstitutions.emplace_back(std::move(sub));
}

auto spp::asts::ClassPrototypeAst::GetClsSymbol() const -> Shared<TypeSymbol> {
  // Getter for the class symbol.
  return _ClsSymbol;
}

static auto IsStdNeverModule(
  Scope const &scope) -> bool {
  // Todo: Maybe remove this and require "!" be used.
  // The namespace names from this scope up to the root, the
  // same parts "FqName" qualifies with, innermost first.
  auto names = spp::Vec<spp::Str>();
  for (auto const *s = &scope; s != nullptr and s->Parent != nullptr; s = s->Parent) {
    if (const auto id = get_if<ScopeIdentifierName>(&s->Name); id != nullptr) {
      names.emplace_back(id->Name->Val);
    }
  }
  return names.size() == 2 and names[0] == "never" and names[1] == "std";
}

auto spp::asts::ClassPrototypeAst::_GenerateSymbols(
  ScopeManager *sm) -> TypeSymbol* {
  auto is_dollar_type = Name->IsCompilerGeneratedType();
  // A template is named as written ("Vec"); as a pattern it is
  // itself over its own parameters, which only "Self" and sup
  // matching need ("TypeSymbol::GnSelfName").
  auto sym_name = AstClone(Name->TypeParts()[0]);
  sym_name->GnArgGroup = GenericArgumentGroupAst::NewEmpty();

  // "!" names the std "Never" class. Every qualified reference
  // to it is built from this name by "FqName", so flagging it
  // makes each one "!" to "TypeEq" - written "!", by name, or
  // through an alias.
  if (sym_name->Name == "Never" and IsStdNeverModule(*sm->CurrentScope->Parent)) {
    sym_name->MarkNeverType();
  }

  // Create the symbols as TypeSymbol pointers, so AliasSymbols
  // can also be used.
  Shared<TypeSymbol> symbol_1 = nullptr;

  // Create the symbol for the type, include generics if
  // applicable, like Vec[T].
  symbol_1 = MakeShared<TypeSymbol>(
    std::move(sym_name), this, sm->CurrentScope, sm->CurrentScope,
    is_dollar_type ? TypeKind::FnMock : TypeKind::Cls, is_dollar_type);
  // Its visibility is read here, where the annotations set it after the symbol is made; its instances read it
  // through it ("TypeSymbol::GetVisibility").
  symbol_1->VisibilityFrom = this;
  sm->CurrentScope->LinkedTypeSymbol = symbol_1;
  sm->CurrentScope->Parent->AddTypeSymbolCheckConflict(symbol_1);
  _ClsSymbol = sm->CurrentScope->LinkedTypeSymbol;

  // A class that still declares parameters is a template, and a
  // template has no layout: its attributes are written in terms
  // of names that stand for nothing yet, so there is no size to
  // give and nothing that can be built against it. Only its
  // instantiations are real types.
  symbol_1->IsConcrete = GnParamGroup->Params.empty();

  return _ClsSymbol.get();
}

/// Why a C struct cannot hold a value of "ref" as a field, or none if it can:
/// a number, a pointer, a fixed array of those, or a class (or tuple) laid out
/// the C way whose own fields it can hold. A generator lowers to a pointer, and
/// a closure or variant to a struct, but C holds none of them.
static auto FfiAttrProblem(
  TypeRef const &ref, ScopeManager const &sm, spp::codegen::LlvmCtx const *ctx)
  -> spp::Opt<spp::StrView> {
  using namespace spp::analyse::utils;
  auto const &scope = *sm.CurrentScope;
  if (type_predicates::IsTypeVoid(ref, scope)) { return spp::Nullopt; }
  if (type_predicates::IsTypeGenerator(ref, scope)) { return "a generator"; }
  if (type_predicates::IsTypeFunction(ref, scope)) { return "a function"; }
  if (type_predicates::IsTypeVariant(ref, scope)) { return "a variant"; }

  const auto type = spp::codegen::GetLlvmTypeOf(ref.WithoutConvention(), ctx);
  if (type_predicates::IsTypeArray(ref, scope)) {
    return FfiAttrProblem(ref.Symbol->AsBound()->TypeArgRefs().front(), sm, ctx);
  }
  if (spp::codegen::IsCScalar(type)) { return spp::Nullopt; }

  // A struct of two or more fields: a tuple is laid out the C way, and
  // a class only if its convention is a foreign one. Either way its own
  // fields have to be ones C can hold too.
  auto const &sym = *ref.Symbol->AsBound();
  const auto is_tuple = type_predicates::IsTypeTuple(ref, scope);
  if (not is_tuple and not spp::codegen::AbiOf(sym).IsForeign()) { return "a class laid out the s++ way"; }
  const auto fields = is_tuple
    ? sym.TypeArgRefs()
    : type_members::GetAllAttrs(sym)
      | genex::views::transform([](auto const &attr) { return spp::get<1>(attr); })
      | genex::to<spp::Vec>();
  for (auto const &field : fields) {
    if (const auto what = FfiAttrProblem(field, sm, ctx); what.has_value()) { return what; }
  }
  return spp::Nullopt;
}

auto spp::asts::ClassPrototypeAst::FillLlvmLayout(
  ScopeManager const *sm, TypeSymbol const *type_sym, codegen::LlvmCtx const *ctx) const -> void {
  IMPORT_UTILS;

  // Todo: error if attribute's default value if a comp generic
  //  value?? Also TEST THIS

  // Non-struct types are compiler known special types, so
  // don't have any field generation. Things like numbers,
  // booleans, functions etc.
  if (type_sym->LlvmInfo->LlvmType == nullptr) {
    codegen::EnsureLlvmTypeComplete(*type_sym, *sm, ctx);
  }
  const auto lt = type_sym->LlvmInfo->LlvmType;
  if (lt == nullptr or not llvm::isa<llvm::StructType>(lt)) {
    return;
  }

  const auto is_empty_pack = type_sym->InstanceOf == nullptr and type_sym->Type != nullptr
    and type_sym->Type->GnParamGroup->Params.size() == 1 and type_sym->Type->GnParamGroup->GetVariadicParam() !=
    nullptr;
  if (not type_sym->IsConcrete and not is_empty_pack) { return; }

  // Next we need to handle tuples (anonymous index-attribute
  // based classes) vs standard struct classes.
  const auto is_tuple = type_predicates::IsTypeTuple(
    TypeRef::ForKindCheck(*type_sym, *sm->CurrentScope), *sm->CurrentScope);
  auto types = Vec<llvm::Type*>();

  // The "Spp" layout sorts the fields by size and alignment, so
  // every field has to be lowered all the way before any of them
  // can be placed - a field still sitting as an opaque placeholder
  // has no size to sort on. "GetLlvmTypeOf" does that on demand, so
  // the order fields are reached in does not matter.
  const auto lower_field = [&](TypeRef const &field) -> llvm::Type* {
    return codegen::GetLlvmTypeOf(field.WithoutConvention(), ctx);
  };

  // Tuple fields are positional based off of the types found
  // in the generic arguments.
  if (is_tuple) {
    const auto elems = type_sym->TypeArgRefs();
    types = elems
      | genex::views::transform([&](auto const &elem) { return lower_field(elem); })
      | genex::to<Vec>();
  }

  // Class attributes are read from the attribute types.
  else {
    types = type_members::GetAllAttrs(*type_sym)
      | genex::views::transform([&](auto const &pair) { return lower_field(spp::get<1>(pair)); })
      | genex::to<Vec>();
  }

  // A class that superimposes one of the "Fun*"/"Gen*" family (eg
  // "Iterator[T]" over "Gen[T]") shares its exact runtime shape too.
  // The fat pointer's fields go ahead of whatever fields this class
  // declares of its own.
  const auto fat_pointer_field_count = type_members::GetSuperimposedFatPointerFieldCount(*type_sym);
  if (fat_pointer_field_count > 0) {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    auto prefixed = Vec<llvm::Type*>(fat_pointer_field_count, ptr_ty);
    prefixed.append_range(types);
    types = std::move(prefixed);
  }

  // An "!ffi" class is laid out as C lays out its struct, so every
  // attribute must be one C can hold. Checked on each instance, as
  // a generic one's attributes are only known there.
  if (not is_tuple and FfiAnnotation != nullptr and genex::all_of(types, [](auto const &x) { return x != nullptr; })) {
    for (auto const &[name, attr_ref, attr_scope] : type_members::GetAllAttrs(*type_sym)) {
      if (const auto what = FfiAttrProblem(attr_ref, *sm, ctx); what.has_value()) {
        const auto attr = type_members::AttrAstOf(*attr_scope, *name);
        Raise<SppFfiAttributeTypeError>(
          ERR_ARGS(*FfiAnnotation, attr != nullptr ? static_cast<Ast const&>(*attr->Type) : *Name, *what));
      }
    }
  }

  // If there are any generic types present (llvm_type is nullptr),
  // skip the layout generation. Tuples use the C convention so they
  // keep the order of types based on the type arguments (standard
  // tuple design); a class uses its own ("!conv", "!ffi").
  if (genex::all_of(types, [](auto const &x) { return x != nullptr; })) {
    auto const &abi = is_tuple ? codegen::GetCAbi() : codegen::AbiOf(*type_sym->AsBound());
    abi.LayOutStruct(*llvm::cast<llvm::StructType>(lt), types, *type_sym->LlvmInfo, *ctx);
  }
}

SPP_MOD_END
