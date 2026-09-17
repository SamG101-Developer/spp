module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.visibility_utils;
import spp.asts.annotation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
TypeStatementAst::TypeStatementAst(
  decltype(Annotations) &&annotations,
  decltype(TokType) &&tok_type,
  decltype(NewType) new_type,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(TokAssign) &&tok_assign,
  decltype(OldType) old_type) :
  Annotations(std::move(annotations)),
  TokType(std::move(tok_type)),
  NewType(std::move(new_type)),
  GnParamGroup(std::move(generic_param_group)),
  TokAssign(std::move(tok_assign)),
  OldType(std::move(old_type)),
  _Generated(false),
  _FromUseStatement(false),
  _AliasSym(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokType, lex::SppTokenType::KW_TYPE, "type");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
  Source.OriginalOldType = AstClone(OldType);
}

TypeStatementAst::~TypeStatementAst() {
}

auto TypeStatementAst::PosStart() const -> std::size_t {
  // Use the "type" token.
  return TokType->PosStart();
}

auto TypeStatementAst::PosEnd() const -> std::size_t {
  // Use the old type.
  return Source.OriginalOldType->PosEnd();
}

auto TypeStatementAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<TypeStatementAst>(
    AstCloneVec(Annotations),
    AstClone(TokType),
    NewType,
    AstClone(GnParamGroup),
    AstClone(TokAssign),
    OldType);
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  ast->_FromUseStatement = _FromUseStatement;
  ast->Visibility = Visibility;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto TypeStatementAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Annotations, "\n");
  SPP_STRING_APPEND_RAW(not Annotations.IsEmpty() ? "\n" : "");
  SPP_STRING_APPEND(TokType).append(" ");
  SPP_STRING_APPEND(NewType);
  SPP_STRING_APPEND(GnParamGroup).append(" ");
  SPP_STRING_APPEND(TokAssign).append(" ");
  SPP_STRING_APPEND(OldType);
  SPP_STRING_END;
}

auto TypeStatementAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Pre-process the annotations.
  Ast::Stage1_PreProcess(ctx);
  for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
}

auto TypeStatementAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Run top level scope generation for the annotations.
  using analyse::errors::SppSecondClassBorrowViolationError;
  using analyse::utils::type_predicates::IsTypeBorrowed;
  for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

  // Check there are no conventions on the new type. Todo: Move to later stage? nothing is loaded in atm
  RaiseIf<SppSecondClassBorrowViolationError>(
    IsTypeBorrowed(*NewType, *sm, false),
    {sm->CurrentScope}, ERR_ARGS(*this, *NewType, "type statement new type"));

  // Create the type symbol for this type, that will point to the old type.
  _AliasSym = MakeShared<TypeSymbol>(
    NewType, nullptr, nullptr, sm->CurrentScope, sm->CurrentScope->ParentModule(), TypeKind::Alias);
  _AliasSym->Alias = MakeShared<AliasInfo>();
  _AliasSym->Alias->Written = OldType;
  // Seeded with the written type, and refined in stage 3 once the chain behind it has been followed. It is never
  // null, because analysing the target is itself what reads it: naming an alias asks the symbol what it resolves to,
  // and that happens while this alias is still being resolved.
  _AliasSym->Alias->Resolved = OldType;
  _AliasSym->Alias->Params = GnParamGroup;
  _AliasSym->Alias->DeclScope = sm->CurrentScope;
  _AliasSym->Alias->FromUseStmt = _FromUseStatement;
  _AliasSym->Alias->Stmt = this;
  sm->CurrentScope->AddTypeSymbolCheckConflict(_AliasSym);

  // Create a new scope for the type statement.
  auto scope_name = ScopeBlockName::FromParts(
    "type-stmt", {NewType.get()}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);

  Ast::Stage2_GenTopLvlScopes(sm, meta);
  GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
  sm->MoveOutOfCurrentScope();

  _Generated = true;
}

auto TypeStatementAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Skip the class scope, and enter the type statement scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  // "Self" is the enclosing sup's type. This stage comes before that name is qualified, so the "Self" symbol is only a
  // provisional one naming the template ("Box"), and an alias of that is of a generic missing its arguments - "T" is
  // then never inferred, or is filled a second time per instance. The sup's name as written ("Box[T]") carries them,
  // bound to the sup's own parameters.
  // Todo: an alias of the sup's own generic class still resolves wrongly in that sup, "Self" or written out: "Box[T]"
  //  leaves "T" uninferred (E72) and "Vec[Box[T]]" becomes "Vec[Box[Box[S32]]]" - SupTypeStatementAstGenericSelfClass,
  //  TestSelfTypePositionsGeneric.test_valid_self_{as_a_type_alias_target,in_a_type_alias_generic_argument}.
  auto *const enclosing = sm->CurrentScope->Parent != nullptr ? sm->CurrentScope->Parent->AstNode : nullptr;
  const auto sup_name = enclosing != nullptr
    and (enclosing->To<SupPrototypeFunctionsAst>() != nullptr or enclosing->To<SupPrototypeExtensionAst>() != nullptr)
    ? AstName(enclosing)
    : nullptr;
  OldType = (sup_name != nullptr and not sup_name->LastTypePart()->GnArgGroup->Args.IsEmpty()
    ? analyse::utils::type_utils::SubstituteSelfTypeWith(*OldType, *sup_name)
    : analyse::utils::type_utils::SubstituteSelfType(*OldType, *sm->CurrentScope, *meta))->WithSourceSpanOf(*OldType);

  // An alias names a type, and a borrow is not one a type can be: it is second class, so it cannot be what a name
  // stands for any more than it can be an attribute or a variant member. The new type is checked at stage 2, where
  // nothing is loaded yet; the old type has to wait until here, because it is a type expression to resolve rather
  // than a name to declare.
  RaiseIf<analyse::errors::SppSecondClassBorrowViolationError>(
    analyse::utils::type_predicates::IsTypeBorrowed(*OldType, *sm, false),
    {sm->CurrentScope}, ERR_ARGS(*this, *OldType, "type statement old type"));

  // Check the "old type" exists (non-generic).
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->SkipTypeAnalysisGenericChecks = true;
    OldType->WithoutGenerics()->Stage7_AnalyseSemantics(sm, meta);
  }

  // Recursively discover the actual type being mapped to.
  auto [mapped_old_type, attach_generics, tracking_scope] = analyse::utils::type_utils::RecursiveAliasSearch(
    *this, _FromUseStatement, sm->CurrentScope->Parent, sm, meta);

  const auto final_sym = sm->CurrentScope->GetTypeSymbol(mapped_old_type->WithoutGenerics().get());
  _AliasSym->Type = final_sym->Type;
  _AliasSym->LinkedScope = final_sym->LinkedScope;
  _AliasSym->InvalidateFqNameCache();
  _AliasSym->DerivesFromSym = final_sym->SharedFromThis<TypeSymbol>();
  _AliasSym->Alias->Resolved = mapped_old_type;
  _AliasSym->Alias->TrackingScope = tracking_scope;

  // An alias of "!" is "!" too, so its own name carries the
  // never flag, as the class's does.
  if (mapped_old_type->IsNeverType()) { NewType->MarkNeverType(); }

  if (attach_generics != nullptr and not attach_generics->Params.IsEmpty()) {
    GnParamGroup = attach_generics;
    GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
    _AliasSym->Alias->Params = GnParamGroup;
    _AliasSym->Alias->ParamsFromTarget = true;
  }
  sm->MoveOutOfCurrentScope();
}

auto TypeStatementAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Skip the class scope, and enter the type statement scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage4_ResolveDeclarations(sm, meta); }

  // The resolved target is what every reader of this alias means by it, so it is what gets qualified and analysed
  // here. What the source wrote stays in "OldType", untouched.
  auto const &alias = *_AliasSym->Alias;

  // Add the "Self" symbol into the scope, mirroring class/sup prototype logic.
  sm->AddSelfTypeSymbol(_AliasSym->LinkedScope, NewType->PosStart());

  // Get the resolved type's symbol, without generics.
  const auto stripped_old_sym = sm->CurrentScope->GetTypeSymbol(alias.Resolved->WithoutGenerics().get(), false);
  if (not stripped_old_sym->IsTypeGeneric()) {
    auto tm = ScopeManager(
      sm->GlobalScope, alias.TrackingScope);
    GnParamGroup->Stage4_ResolveDeclarations(alias.ParamsFromTarget ? &tm : sm, meta);
    // Stamped from the scope of the lowest level alias, where its names mean what they were written to.
    analyse::utils::type_utils::StampWrittenParts(*alias.Resolved, *alias.TrackingScope);
    alias.Resolved->Stage7_AnalyseSemantics(sm, meta); // Analyse in this scope (generics are in this scope)

    const auto old_sym = sm->CurrentScope->GetTypeSymbol(alias.Resolved.get());

    // The target means what it resolved to here from wherever the alias is read, as a class's own name does
    // ("TypeSymbol::FqName"), rather than whatever its spelling names there.
    if (alias.Resolved->Stamp() == nullptr and old_sym->Kind == TypeKind::Class
      and old_sym->Alias == nullptr) { alias.Resolved->SetStamp(old_sym); }
    _AliasSym->Type = old_sym->Type;
    _AliasSym->LinkedScope = old_sym->LinkedScope;
    _AliasSym->InvalidateFqNameCache();
    _AliasSym->DerivesFromSym = old_sym->SharedFromThis<TypeSymbol>();
    _AliasSym->LlvmInfo = old_sym->LlvmInfo;
  }
  sm->MoveOutOfCurrentScope();
}

auto TypeStatementAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }

  // Sync the alias symbol's visibility from the AST (annotations set Visibility in Stage5). Without this the alias
  // symbol keeps the symbol-constructor default (public), making every alias publicly accessible regardless of its
  // annotation (or lack of one: unannotated statements are private).
  if (_AliasSym != nullptr) {
    _AliasSym->Visibility = Visibility.first;
    for (auto const &[_, inst] : _AliasSym->Instances) { inst->Visibility = Visibility.first; }
  }

  sm->MoveOutOfCurrentScope();
}

auto TypeStatementAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *) -> void {
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  sm->MoveOutOfCurrentScope();
}

auto TypeStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  using analyse::utils::generic_bindings::EnforceGenericConstraintsOfParams;
  using analyse::utils::visibility_utils::CheckModuleTypeVisibility;
  for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }

  // If this is a pre-generated AST (mod/sup context), skip any generation steps.
  if (_Generated) {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    auto const &resolved = _AliasSym->Alias->Resolved;
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->AllowAbstractType = true;
      resolved->ResetCache();
      resolved->Stage7_AnalyseSemantics(sm, meta);
    }

    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(resolved.get());
    if (cls_sym != nullptr and cls_sym->Type) {
      EnforceGenericConstraintsOfParams(*cls_sym, *GnParamGroup, *sm, *meta);
    }

    // Check visibility here specifically (almost always done in TypeIdentifierAst) because of the source type
    // auto expansion.
    const auto named_target_sym = sm->CurrentScope->GetTypeSymbol(Source.OriginalOldType->WithoutGenerics().get());
    if (named_target_sym != nullptr and named_target_sym->ScopeDefinedIn != nullptr) {
      CheckModuleTypeVisibility(
        *named_target_sym, *Source.OriginalOldType, *named_target_sym->ScopeDefinedIn, *sm, *meta);
    }

    sm->MoveOutOfCurrentScope();
    return;
  }

  // Otherwise, run all generation steps.
  const auto current_scope = sm->CurrentScope;
  auto iter_copy = sm->CurrentIterator();

  sm->Reset(current_scope, iter_copy);
  iter_copy = sm->CurrentIterator();
  Stage2_GenTopLvlScopes(sm, meta);

  sm->Reset(current_scope, iter_copy);
  iter_copy = sm->CurrentIterator();
  Stage3_GenTopLvlAliases(sm, meta);

  // sm->Reset(current_scope, iter_copy);
  // Stage4_ResolveDeclarations(sm, meta);
}

auto TypeStatementAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *) -> void {
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  sm->MoveOutOfCurrentScope();
}

auto TypeStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
  sm->MoveOutOfCurrentScope();
}

auto TypeStatementAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *, codegen::LlvmCtx *) -> llvm::Value* {
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto TypeStatementAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *, codegen::LlvmCtx *) -> llvm::Value* {
  sm->MoveToNextScope();
  // SPP_ASSERT(sm->CurrentScope == _Scope);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto TypeStatementAst::MarkFromUseStatement() -> void {
  _FromUseStatement = true;
}

auto TypeStatementAst::IsFromUseStatement() const -> bool {
  return _FromUseStatement;
}

SPP_MOD_END
