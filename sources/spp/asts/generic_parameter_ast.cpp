module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_parameter_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.annotation_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;
import spp.asts.utils.visibility;
import spp.lex.tokens;

SPP_MOD_BEGIN
GenericParameterAst::GenericParameterAst(
  decltype(TokCmp) &&tok_cmp,
  decltype(TokEllipsis) &&tok_ellipsis,
  decltype(Name) name,
  decltype(TypeConstraints) &&constraints,
  decltype(TokColon) &&tok_colon,
  decltype(CompType) comp_type,
  decltype(TokAssign) &&tok_assign,
  decltype(TypeDefault) type_default,
  decltype(CompDefault) comp_default) :
  OrderableAst(
    tok_ellipsis != nullptr
    ? utils::OrderableTag::kVariadicParam
    : type_default != nullptr or comp_default != nullptr
    ? utils::OrderableTag::kOptionalParam
    : utils::OrderableTag::kRequiredParam),
  TokCmp(std::move(tok_cmp)),
  TokEllipsis(std::move(tok_ellipsis)),
  Name(std::move(name)),
  TypeConstraints(std::move(constraints)),
  TokColon(std::move(tok_colon)),
  CompType(std::move(comp_type)),
  TokAssign(std::move(tok_assign)),
  TypeDefault(std::move(type_default)),
  CompDefault(std::move(comp_default)),
  WrittenCompDefault(CompDefault != nullptr ? AstCloneShared(CompDefault.get()) : nullptr),
  _DummyScopes({}) {
  // Default the tokens and constraints of the parameter's kind.
  using lex::SppTokenType;
  if (IsCompParam()) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCmp, SppTokenType::KW_CMP, "cmp");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokColon, SppTokenType::TK_COLON, ":");
  }
  else {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TypeConstraints);
  }
  if (IsOptional()) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, SppTokenType::TK_ASSIGN, "=");
  }
}

GenericParameterAst::~GenericParameterAst() = default;

auto GenericParameterAst::IsTypeParam() const -> bool {
  return CompType == nullptr;
}

auto GenericParameterAst::IsCompParam() const -> bool {
  return CompType != nullptr;
}

auto GenericParameterAst::IsVariadic() const -> bool {
  return TokEllipsis != nullptr;
}

auto GenericParameterAst::IsOptional() const -> bool {
  return TypeDefault != nullptr or CompDefault != nullptr;
}

auto GenericParameterAst::IsRequired() const -> bool {
  return not IsVariadic() and not IsOptional();
}

auto GenericParameterAst::PosStart() const -> std::size_t {
  // Use the first token: "cmp", "..", or the name.
  if (TokCmp != nullptr) { return TokCmp->PosStart(); }
  if (IsVariadic()) { return TokEllipsis->PosStart(); }
  return Name->PosStart();
}

auto GenericParameterAst::PosEnd() const -> std::size_t {
  // Use the default, then a comp parameter's type, then the
  // ".." of a variadic type parameter, then the name.
  if (TypeDefault != nullptr) { return TypeDefault->PosEnd(); }
  if (CompDefault != nullptr) { return CompDefault->PosEnd(); }
  if (IsCompParam()) { return CompType->PosEnd(); }
  if (IsVariadic()) { return TokEllipsis->PosEnd(); }
  return Name->PosEnd();
}

auto GenericParameterAst::ShareParamId(
  GenericParameterAst const &that) -> void {
  _ParamId = that._ParamId;
}

auto GenericParameterAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<GenericParameterAst>(
    AstClone(TokCmp),
    AstClone(TokEllipsis),
    AstCloneShared(Name),
    AstClone(TypeConstraints),
    AstClone(TokColon),
    AstCloneShared(CompType),
    AstClone(TokAssign),
    AstCloneShared(TypeDefault),
    AstCloneShared(CompDefault));
  ast->IsInherited = IsInherited;
  ast->WrittenCompDefault = WrittenCompDefault;
  ast->_ParamId = _ParamId;
  return ast;
}

auto GenericParameterAst::ToString() const -> Str {
  SPP_STRING_START;
  if (IsCompParam()) {
    SPP_STRING_APPEND(TokCmp).append(" ");
    if (IsVariadic()) { SPP_STRING_APPEND(TokEllipsis); }
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(CompType);
    if (IsOptional()) {
      SPP_STRING_APPEND_RAW(" ");
      SPP_STRING_APPEND(TokAssign).append(" ");
      SPP_STRING_APPEND(CompDefault);
    }
  }
  else {
    if (IsVariadic()) { SPP_STRING_APPEND(TokEllipsis); }
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TypeConstraints);
    if (IsOptional()) {
      SPP_STRING_APPEND(TokAssign);
      SPP_STRING_APPEND(TypeDefault);
    }
  }
  SPP_STRING_END;
}

auto GenericParameterAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *) -> void {
  using utils::Visibility;

  // An inherited parameter names its "sup" block's symbol; a
  // copy here would shadow it.
  if (IsInherited) { return; }

  // A comp parameter is a variable symbol for its constant in
  // the current scope (class / function).
  if (IsCompParam()) {
    auto sym = MakeUnique<VariableSymbol>(
      IdentifierAst::FromType(*Name), CompType, sm->CurrentScope,
      VariableKind::GnCompParam, false, Visibility::kPublic);

    sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
    if (*_ParamId == 0) { *_ParamId = NextGnParamId(); }
    sym->OwnParamId = *_ParamId;
    sym->IsVariadic = IsVariadic();
    RegisterGnCompParam(*sym);

    // Recorded as a type parameter's is (below), in the comp side's own slot: its name is an identifier, which
    // records its comp identity ("IdentifierAst::StampedCompId").
    sym->Name->StampCompId(ParamCompId(*_ParamId));
    sm->CurrentScope->AddVarSymbol(std::move(sym));
    return;
  }

  // A type parameter gets a dummy scope for the generic type.
  auto dummy_scope_name = ScopeBlockName::FromParts(
    "generic-parameter-type", {Name->LastTypePart()}, PosStart());
  auto dummy_ast = MakeUnique<ClassPrototypeAst>(
    SPP_NO_ANNOTATIONS, nullptr, nullptr, nullptr, nullptr);
  auto dummy_scope = MakeUnique<Scope>(
    dummy_scope_name, sm->CurrentScope, dummy_ast.get());
  _DummyScopeAsts.EmplaceBack(std::move(dummy_ast));

  // Create the type symbol for the generic parameter.
  const auto sym = MakeShared<TypeSymbol>(
    AstCloneShared(Name->LastTypePart()), nullptr, dummy_scope.get(),
    sm->CurrentScope, TypeKind::GnTypeParam, false, Visibility::kPublic,
    ConventionTag::MOV, TypeConstraints->Constraints);
  sym->IsVariadic = IsVariadic();
  if (*_ParamId == 0) { *_ParamId = NextGnParamId(); }
  sym->OwnParamId = *_ParamId;
  RegisterGnTypeParam(*sym);

  // The declaration names this parameter, and so does every
  // argument group built from it: "FromParams" shares this
  // node, "GenericArgumentAst::FromSymbol" the symbol's. Stamping
  // both lets substitution match an occurrence to the argument
  // standing for it by identity ("ParamId"), rather than by
  // the name the two happen to share.
  Name->StampTypeId(NameTypeIdOf(*sym));
  sym->Name->StampTypeId(NameTypeIdOf(*sym));
  sm->CurrentScope->AddTypeSymbol(sym);

  dummy_scope->LinkedTypeSymbol = sym;
  _DummyScopes.EmplaceBack(dummy_scope.get());
  ScopeManager::TempScopes.EmplaceBack(
    std::move(dummy_scope));
}

auto GenericParameterAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // An optional type parameter analyses its default where it
  // is written and records what it means there
  // ("type_resolution::StampTypeParts"), as it is read from
  // wherever the parameter is bound.
  if (IsTypeParam()) {
    if (IsOptional()) {
      TypeDefault->Stage7_AnalyseSemantics(sm, meta);
      type_resolution::StampTypeParts(*TypeDefault, *sm->CurrentScope);
    }
    return;
  }

  // Resolve the type on the comp parameter, containing whatever
  // its analysis sets.
  const auto _meta_guard = MetaGuard(meta);

  // Check the type exists and qualify.
  // Todo: a method's "cmp p: Box[T]" (or "Self") in a generic
  //  sup keeps the sup's "T", unknown at the call (E26, located
  //  in std) - GenericParameterCompGenericClass.test_valid_comp_parameter_typed_by_the_class_generic.
  CompType = type_resolution::AnalyseWrittenType(*CompType, *sm, *meta);

  // The default records the comp generics it names where it
  // is written, as a type default records its parts: it is
  // read from wherever the parameter is bound.
  if (WrittenCompDefault != nullptr) { type_resolution::StampCompParts(*WrittenCompDefault, *sm->CurrentScope); }
  if (not IsInherited) {
    const auto sym = sm->CurrentScope->FindVarSymbol(
      IdentifierAst::FromType(*Name).get());
    sym->Type = CompType;
  }

  // Ensure that the convention type doesn't have a
  // convention, as this violates second class borrow
  // rules.
  RaiseIf<SppSecondClassBorrowViolationError>(
    type_predicates::IsTypeBorrowed(*CompType, *sm), {sm->CurrentScope},
    ERR_ARGS(*CompType, *CompType, "generic comp argument"));
}

auto GenericParameterAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // A type parameter analyses its name and any default.
  // Its constraints were analysed in stage 4, before sup
  // scopes load, which is too early for the constraints
  // of their own generic arguments, so they are analysed
  // again now that those can be checked.
  if (IsTypeParam()) {
    Name->Stage7_AnalyseSemantics(sm, meta);
    if (IsOptional()) { TypeDefault->Stage7_AnalyseSemantics(sm, meta); }
    if (TypeConstraints != nullptr) {
      const auto _meta_guard = MetaGuard(meta);
      meta->AllowAbstractType = true;
      for (auto const &constraint : TypeConstraints->Constraints) {
        constraint->ResetCache();
        constraint->Stage7_AnalyseSemantics(sm, meta);
      }
    }
    return;
  }

  // An optional comp parameter analyses its default, and
  // makes sure it is of the parameter's type.
  if (not IsOptional()) { return; }
  CompDefault->Stage7_AnalyseSemantics(sm, meta);
  if (not type_compare::Assignable(
    TypeRef::Of(*CompType, *sm->CurrentScope), CompDefault->InferTypeRef(sm, meta),
    *sm->CurrentScope, *sm->CurrentScope)) {
    const auto default_type = CompDefault->InferType(sm, meta);
    Raise<SppTypeMismatchError>({sm->CurrentScope}, ERR_ARGS(*CompType, *CompType, *CompDefault, *default_type));
  }
}

auto GenericParameterAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Only an optional comp parameter's default holds memory to
  // check.
  if (not IsCompParam() or not IsOptional()) {
    Ast::Stage8_CheckMemory(sm, meta);
    return;
  }
  CompDefault->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(*CompDefault, *CompDefault, *sm, meta);
}

auto GenericParameterAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // A comp parameter resolves to its own name.
  if (IsTypeParam()) {
    Ast::Stage9_CompTimeResolve(sm, meta);
    return;
  }
  meta->CompTimeResult = IdentifierAst::FromType(*Name);
}

auto GenericParameterAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Only a comp parameter generates anything.
  if (IsTypeParam()) { return Ast::Stage11_CodeGen(sm, meta, ctx); }

  // The compile time constants' symbols need to be allocated
  // into the function. Start with "nullptr" value, to generate
  // the alloca. Generic instantiations of these functions will
  // then inject in the generic argument translations.
  auto cast_name = IdentifierAst::FromType(*Name);
  const auto sym = sm->CurrentScope->FindVarSymbol(cast_name.get());
  const auto held_type = sym != nullptr and sym->Type != nullptr ? sym->Type : CompType;
  const auto cmp = MakeUnique<CmpStatementAst>(
    SPP_NO_ANNOTATIONS, nullptr, std::move(cast_name), nullptr, held_type, nullptr, nullptr);
  cmp->Stage10_PreCodeGen(sm, meta, ctx);
  return nullptr;
}

auto GenericParameterAst::GetDummyScopes() const -> std::span<Scope* const> {
  // View the dummy scope vector.
  return _DummyScopes.ToView();
}

auto GenericParameterAst::ClearDummyScopes() -> void {
  // Clear the dummy scopes.
  _DummyScopeAsts.Clear();
}

SPP_MOD_END
