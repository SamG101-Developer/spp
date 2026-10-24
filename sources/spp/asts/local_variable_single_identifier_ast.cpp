module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.local_variable_single_identifier_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.borrows;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.LlvmMaterialize;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coerce;
import spp.codegen.llvm_type;
import spp.utils.ice;
import spp.utils.uid;

SPP_MOD_BEGIN
LocalVariableSingleIdentifierAst::LocalVariableSingleIdentifierAst(
  decltype(TokMut) &&tok_mut,
  decltype(Name) name,
  decltype(Alias) &&alias) :
  TokMut(std::move(tok_mut)),
  Name(std::move(name)),
  Alias(std::move(alias)) {
}

LocalVariableSingleIdentifierAst::~LocalVariableSingleIdentifierAst() = default;

auto LocalVariableSingleIdentifierAst::PosStart() const -> std::size_t {
  // Use the "mut" token or name.
  return TokMut ? TokMut->PosStart() : Name->PosStart();
}

auto LocalVariableSingleIdentifierAst::PosEnd() const -> std::size_t {
  // Use the alias or name.
  return Alias ? Alias->PosEnd() : Name->PosEnd();
}

auto LocalVariableSingleIdentifierAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto l = MakeUnique<LocalVariableSingleIdentifierAst>(
    AstClone(TokMut),
    AstCloneShared(Name),
    AstClone(Alias));
  l->Conv = AstClone(Conv);
  l->_FromCasePattern = _FromCasePattern;
  return l;
}

auto LocalVariableSingleIdentifierAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokMut).append(TokMut ? " " : "");
  SPP_STRING_APPEND(Name).append(Alias ? " " : "");
  SPP_STRING_APPEND(Alias);
  SPP_STRING_END;
}

auto LocalVariableSingleIdentifierAst::BindsByMove() const -> bool {
  // A name binds what it stands for, unless it asks for it through a borrow, which leaves the value where it was.
  return Conv == nullptr;
}

auto LocalVariableSingleIdentifierAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Get the value and its type from the "meta" information.
  const auto val = meta->LetStatementFromUninitialized
    ? nullptr
    : meta->LetStatementValue;

  const auto val_type = meta->LetStatementValue != nullptr
    ? meta->LetStatementValue->InferType(sm, meta)
    : nullptr;

  // Create a variable symbol for this identifier and value.
  const auto sym_name = Alias != nullptr ? Alias->Name : Name;
  auto sym = MakeShared<VariableSymbol>(
    sym_name,
    meta->LetStatementExplicitType != nullptr ? meta->LetStatementExplicitType : val_type,
    sm->CurrentScope,
    sym_name->Val.starts_with("$") ? VariableKind::Temporary : VariableKind::Local,
    TokMut != nullptr or (Conv and *Conv == ConventionTag::MUT));

  // Update the type if there is a convention present.
  if (Conv != nullptr) {
    sym->Type = sym->Type->WithConvention(AstClone(Conv));
  }

  // Set the initialization AST (for errors).
  analyse::utils::memory_state::StateOf(*sm->CurrentScope, *sym).AstInitialization = Name.get();
  analyse::utils::memory_state::StateOf(*sm->CurrentScope, *sym).AstInitializationOrigin = Name.get();

  if (val != nullptr) {
    // Set borrow asts based on the value's type's convention.
    if (const auto conv = val_type->GetConvention(); conv != nullptr) {
      sym->MemInfo->AstBorrowed = val;
    }
  }

  else {
    // Mark an uninitialized variable as "moved"
    analyse::utils::memory_state::StateOf(*sm->CurrentScope, *sym).AstMoved = this;
  }

  // Add the symbol to the current scope, and record it as
  // this scope's "let" binding: every later walk declares
  // it again here, rather than finding whichever binding
  // of the name the scope declared last (shadowing issues).
  _Symbol = sym;
  sm->CurrentScope->RecordLetBinding(sym);
  sm->CurrentScope->AddVarSymbol(std::move(sym));
}

auto LocalVariableSingleIdentifierAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // No value => nothing to check, only the binding to declare.
  IMPORT_UTILS;
  if (meta->LetStatementFromUninitialized) {
    ignore = _Declare(sm);
    return;
  }

  // The value is checked before this binding is declared, so the name means whatever it did before ("let x = f(x)"),
  // and anything the value does on the way - an early "ret" out of a "?" - happens with this binding not yet holding
  // anything. What the value hands a borrow to (a coroutine handle, a closure) finds this binding through
  // "AssignmentTargetSymbol". A binding written with a borrow convention borrows its value rather than taking it:
  // "is Some[T](&val)" looks at the payload, it does not move it off the subject.
  const auto saved_target = meta->AssignmentTargetSymbol;
  meta->AssignmentTargetSymbol = _Symbol.get();
  meta->LetStatementValue->Stage8_CheckMemory(sm, meta);
  meta->AssignmentTargetSymbol = saved_target;

  const auto borrows = Conv != nullptr;
  mem_utils::ValidateSymbolMemory(
    *meta->LetStatementValue, *this, *sm, meta,
    {.CheckMove = not borrows, .CheckMoveFromBorrowedCtx = not borrows, .MarkMoves = not borrows});

  // A binding shadowed in its own scope can never be named again, so its value has to have been consumed by now - the
  // value just checked may be what consumed it ("let x = f(x)").
  const auto sym_name = Alias != nullptr ? Alias->Name.get() : Name.get();
  if (auto const *const shadowed = sm->CurrentScope->FindVarSymbol(sym_name, true); shadowed != nullptr) {
    linear_utils::CheckOverwrite(*shadowed, *this, "Shadowing binding", *sm);
  }

  // A holder moved in whole by name hands its loans to this binding (a borrow a generator yielded, moved on). Found
  // before this binding is declared, which may shadow it.
  auto const *const moved_from = not borrows and meta->LetStatementValue->To<IdentifierAst>() != nullptr
    ? sm->CurrentScope->FindVarSymbolOutermost(*meta->LetStatementValue).first
    : nullptr;

  // Declare the binding, holding its value; what the value lent to it is held by it from here.
  const auto sym = _Declare(sm);
  memory_state::StateOf(*sm->CurrentScope, *sym).InitializedBy(*Name);
  auto &loans = memory_state::LoansOf(*sm->CurrentScope);
  memory_state::ActivateLoans(loans, *sym);
  if (moved_from != nullptr) { memory_state::TransferLoans(loans, *moved_from, *sym); }

  // What the value borrows has to outlive this binding: a block value can borrow what it declares itself.
  borrows::EnforceBorrowsOutliveHolder(*sym, moved_from, *this, *sm);

  // A binding that borrows its value ("is A(&mut n, ..)") holds a loan of the place it names, to its last use.
  if (Conv != nullptr) {
    sym->MemInfo->AstBorrowed = Conv.get();
    loans.emplace_back(memory_state::MakeLoan(
      regions::PlaceAtOf(*meta->LetStatementValue, *sm->CurrentScope), *Conv == ConventionTag::MUT, sym));
  }
}

auto LocalVariableSingleIdentifierAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // The value is resolved before this binding is declared, then assigned into it.
  const auto _meta_guard = MetaGuard(meta);
  meta->AssignmentTarget = Alias != nullptr ? Alias->Name : Name;
  meta->LetStatementValue->Stage9_CompTimeResolve(sm, meta);
  if (const auto sym = _Declare(sm); sym != nullptr) {
    // Can be nullptr for the materialization into $ symbols.
    sym->CompTimeValue = std::move(meta->CompTimeResult);
  }
}

auto LocalVariableSingleIdentifierAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  // Create the alloca for the variable.
  const auto uid = "." + Uid();
  const auto borrows = Conv != nullptr;
  const auto llvm_type = borrows
    ? static_cast<llvm::Type*>(llvm::PointerType::get(*ctx->Context, 0))
    : meta->LetStatementPrecomputedValue != nullptr
    ? meta->LetStatementPrecomputedValue->getType()
    : codegen::GetLlvmTypeOf(TypeRef::Of(*meta->LetStatementExplicitType, *sm->CurrentScope), ctx);
  SPP_ASSERT(llvm_type != nullptr);

  // The storage for this variable. Normally a fresh alloca at the top of the function, but inside a coroutine the
  // resume prologue has already pointed the symbol at its env field (its frame lives on the caller's stack). Reuse
  // that pre-set alloca storage instead of allocating a fresh non-persisting slot.
  const auto var_sym = _Binding(sm);

  // Void could have been introduced via a generic implementation,
  // so just prevent allocas from Void types.
  const auto is_void = codegen::IsValuelessType(llvm_type);
  auto alloca = var_sym->LlvmInfo->Alloca;
  if (alloca == nullptr and not is_void) {
    alloca = codegen::LlvmEntryAlloca(llvm_type, "local.alloca" + Name->Val + "." + uid, ctx);
    var_sym->LlvmInfo->Alloca = alloca;
  }

  // Generate the initializer expression. A function/closure
  // parameter has no initializer expression to codegen - its
  // value is an already-generated llvm::Argument, so that takes
  // priority over evaluating "LetStatementValue".
  if (meta->LetStatementPrecomputedValue != nullptr and not is_void) {
    ctx->Builder.CreateStore(meta->LetStatementPrecomputedValue, alloca);
  }
  else if (not meta->LetStatementFromUninitialized) {
    const auto _meta_guard = MetaGuard(meta);
    meta->AssignmentTarget = Alias != nullptr ? Alias->Name : Name;
    meta->LlvmAssignmentTarget = alloca;

    // What a borrow binds is the address of the value, not
    // the value: the same lowering a borrow argument gets.
    auto llvm_val = borrows
      ? codegen::LlvmAddrOf(*meta->LetStatementValue, sm, meta, ctx)
      : meta->LetStatementValue->Stage11_CodeGen(sm, meta, ctx);

    // Read before this binding is declared: "let val = val.val"
    // would otherwise ask the new "val" for a "val" attribute.
    const auto value_ref = meta->LetStatementValue->InferTypeRef(sm, meta);

    // The declared type may be a variant that the initializer
    // is only a member of ("let x: Opt[S32] = Some(val=1)"),
    // in which case the value has to be tagged and copied into
    // the payload. Storing it raw put the member at offset zero,
    // on top of the tag, so the slot read back as whatever the
    // member's first bytes happened to be.
    // A "!" value ("let y = loop true { .. }") never arrives, so
    // there is nothing to coerce or store.
    const auto diverged = llvm_val == nullptr;
    if (not is_void and not borrows and not diverged and meta->LetStatementExplicitType != nullptr) {
      llvm_val = codegen::CoerceToSlot(
        llvm_val, TypeRef::Of(*meta->LetStatementExplicitType, *sm->CurrentScope), value_ref, *sm,
        "local.variant" + uid, ctx);
    }

    // Skip storing Void (created via generic implementation
    // analysis).
    if (not is_void and not diverged) { ctx->Builder.CreateStore(llvm_val, alloca); }
  }

  // Declare the binding, now its value is in place.
  ignore = _Declare(sm);

  // Alloca already added; return nullptr.
  return nullptr;
}

auto LocalVariableSingleIdentifierAst::_Own(
  ScopeManager const *sm) const -> VariableSymbol* {
  // A binding only ever analysed as a clone of another has no symbol of its own; the one its name finds is it.
  if (_Symbol != nullptr) { return _Symbol.get(); }
  return sm->CurrentScope->FindVarSymbol(Alias != nullptr ? Alias->Name.get() : Name.get());
}

auto LocalVariableSingleIdentifierAst::_Binding(
  ScopeManager const *sm) const -> VariableSymbol* {
  // A clone walked over its original's scope has no symbol of its own, and its binding is hidden until declared: it is
  // the one its original recorded there, the first of its name not already visible (the next, for a "let" shadowing
  // another of its name in the same scope).
  if (_Symbol != nullptr) { return _Symbol.get(); }
  auto const &scope = *sm->CurrentScope;
  auto const *const name = Alias != nullptr ? Alias->Name.get() : Name.get();
  auto const *const visible = scope.FindVarSymbol(name, true);
  auto past_visible = visible == nullptr;
  for (auto const &recorded : scope.LetBindings) {
    if (recorded->Name->NameId() != name->NameId()) { continue; }
    if (past_visible) { return recorded.get(); }
    past_visible = recorded.get() == visible;
  }
  return _Own(sm);
}

auto LocalVariableSingleIdentifierAst::_Declare(
  ScopeManager *sm) const -> VariableSymbol* {
  // Put this binding back under its name, from here on in the scope.
  auto *const sym = _Binding(sm);
  if (sym != nullptr) { sm->CurrentScope->AddVarSymbol(sym->SharedFromThis<VariableSymbol>()); }
  return sym;
}

auto LocalVariableSingleIdentifierAst::Narrow(
  Shared<VariableSymbol> const &flow, Scope &scope) -> void {
  // The scope's record follows, so entering it hides the narrowed symbol, as it did the original.
  for (auto &recorded : scope.LetBindings) { if (recorded == _Symbol) { recorded = flow; } }
  _Symbol = flow;
  scope.AddVarSymbol(flow);
}

auto LocalVariableSingleIdentifierAst::ExtractNames() const -> Vec<Shared<IdentifierAst>> {
  // Return the single name as a vector that can get appended to
  // from nesting.
  return {Name};
}

auto LocalVariableSingleIdentifierAst::ExtractName() const -> Shared<IdentifierAst> {
  // Return the single name (identifier) for matching capability.
  return Name;
}

SPP_MOD_END
