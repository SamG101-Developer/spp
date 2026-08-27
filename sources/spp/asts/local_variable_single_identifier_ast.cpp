module;
#include <spp/macros.hpp>

module spp.asts.local_variable_single_identifier_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_type;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::LocalVariableSingleIdentifierAst::LocalVariableSingleIdentifierAst(
  decltype(TokMut) &&tok_mut,
  decltype(Name) name,
  decltype(Alias) &&alias) :
  TokMut(std::move(tok_mut)),
  Name(std::move(name)),
  Alias(std::move(alias)) {
}

spp::asts::LocalVariableSingleIdentifierAst::~LocalVariableSingleIdentifierAst() = default;

auto spp::asts::LocalVariableSingleIdentifierAst::PosStart() const
  -> std::size_t {
  // Use the "mut" token or name.
  return TokMut ? TokMut->PosStart() : Name->PosStart();
}

auto spp::asts::LocalVariableSingleIdentifierAst::PosEnd() const
  -> std::size_t {
  // Use the alias or name.
  return Alias ? Alias->PosEnd() : Name->PosEnd();
}

auto spp::asts::LocalVariableSingleIdentifierAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto l = MakeUnique<LocalVariableSingleIdentifierAst>(
    AstClone(TokMut),
    AstCloneShared(Name),
    AstClone(Alias));
  l->Conv = AstClone(Conv);
  l->_FromCasePattern = _FromCasePattern;
  return l;
}

auto spp::asts::LocalVariableSingleIdentifierAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokMut).append(TokMut ? " " : "");
  SPP_STRING_APPEND(Name).append(Alias ? " " : "");
  SPP_STRING_APPEND(Alias);
  SPP_STRING_END;
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Get the value and its type from the "meta" information.
  const auto val = meta->LetStatementFromUninitialized
    ? nullptr
    : meta->LetStatementValue;

  const auto val_type = meta->LetStatementValue != nullptr
    ? meta->LetStatementValue->InferType(sm, meta)
    : nullptr;

  // Create a variable symbol for this identifier and value.
  auto sym = MakeShared<analyse::scopes::VariableSymbol>(
    Alias != nullptr ? Alias->Name : Name,
    meta->LetStatementExplicitType != nullptr ? meta->LetStatementExplicitType : val_type,
    sm->CurrentScope, TokMut != nullptr or (Conv and *Conv == ConventionTag::MUT));

  // Update the type if there is a convention present.
  if (Conv != nullptr) {
    sym->Type = sym->Type->WithConvention(AstClone(Conv));
  }

  // Set the initialization AST (for errors).
  sym->MemInfo->AstInitialization = {Name.get(), sm->CurrentScope};
  sym->MemInfo->AstInitializationOrigin = {Name.get(), sm->CurrentScope};

  // Increment the initialization counter for initialized
  // statements.
  if (val != nullptr) {
    sym->MemInfo->InitializationCounter = 1;

    // Set borrow asts based on the value's type's convention.
    if (const auto conv = val_type->GetConvention(); conv != nullptr) {
      sym->MemInfo->AstBorrowed = {val, sm->CurrentScope};
    }
  }

  else {
    // Mark an uninitialized variable as "moved"
    sym->MemInfo->AstMoved = {this, sm->CurrentScope};
  }

  // Add the symbol to the current scope.
  sm->CurrentScope->AddVarSymbol(std::move(sym));
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // No value => nothing to check.
  using analyse::utils::mem_utils::ValidateSymbolMemory;
  if (meta->LetStatementFromUninitialized) { return; }

  // Check the value's memory. A variable does not hold anything until its value has been evaluated, so whatever the
  // value does on the way - an early "ret" out of a "?", a "case" branch that returns - happens with this symbol still
  // empty. Stage 7 fills the initialization ast in for error reporting, well before any of that is known, so it is
  // taken back down for the duration of the value's own check and restored after: the linearity walk at a "ret" would
  // otherwise report the variable being declared here as a value that "ret" abandoned.
  const auto pre_sym = sm->CurrentScope->GetVarSymbol(Alias != nullptr ? Alias->Name.get() : Name.get());
  const auto pre_init = pre_sym != nullptr
    ? pre_sym->MemInfo->AstInitialization
    : decltype(pre_sym->MemInfo->AstInitialization)();
  if (pre_sym != nullptr) { pre_sym->MemInfo->AstInitialization = {nullptr, nullptr}; }
  meta->LetStatementValue->Stage8_CheckMemory(sm, meta);
  if (pre_sym != nullptr) { pre_sym->MemInfo->AstInitialization = pre_init; }

  // Fix variable shadowing, where a newer version of the symbol is
  // gotten because stage7 added it, when we are trying to use the
  // original.
  const auto sym_name = Alias != nullptr ? Alias->Name.get() : Name.get();
  const auto shadowed = sm->CurrentScope->RemVarSymbol(sym_name);

  // A binding written with a borrow convention borrows its
  // value rather than taking it: "is Some[T](&val)" looks at
  // the payload, it does not move it off the subject. Without
  // this, the read is recorded as a move whatever the binding
  // says, which leaves the subject partially initialized.
  const auto borrows = Conv != nullptr;
  ValidateSymbolMemory(
    *meta->LetStatementValue, *this, *sm, not borrows, true, not borrows, not borrows, meta);
  if (shadowed != nullptr) { sm->CurrentScope->AddVarSymbol(shadowed); }

  // Get the name or alias symbol to mark it as initialized.
  const auto sym = sm->CurrentScope->GetVarSymbol(sym_name);
  sym->MemInfo->InitializedBy(*Name, sm->CurrentScope);
  if (Conv != nullptr) {
    sym->MemInfo->AstBorrowed = {Conv.get(), sm->CurrentScope};
  }
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Assign the generated value into the variable symbol.
  meta->Save();
  meta->AssignmentTarget = Alias != nullptr ? Alias->Name : Name;

  // Fix variable shadowing, where a newer version of the symbol is
  // gotten because stage7 added it, when we are trying to use the
  // original.
  const auto sym_name = Alias != nullptr ? Alias->Name.get() : Name.get();
  const auto shadowed = sm->CurrentScope->RemVarSymbol(sym_name);
  meta->LetStatementValue->Stage9_CompTimeResolve(sm, meta);
  if (shadowed != nullptr) { sm->CurrentScope->AddVarSymbol(shadowed); }

  const auto var_sym = sm->CurrentScope->GetVarSymbol(sym_name);
  if (var_sym != nullptr) {
    // Can be nullptr for the materialization into $ symbols.
    var_sym->CompTimeValue = std::move(meta->CmpResult);
  }
  meta->Restore();
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Create the alloca for the variable.
  const auto uid = "." + spp::utils::Uid(this);
  const auto llvm_type = meta->LetStatementPrecomputedValue != nullptr
    ? meta->LetStatementPrecomputedValue->getType()
    : codegen::GetLlvmTypeOf(*meta->LetStatementExplicitType, *sm->CurrentScope, ctx);
  SPP_ASSERT(llvm_type != nullptr);

  // The storage for this variable. Normally a fresh alloca at the top of the function, but inside a coroutine the
  // resume prologue has already pointed the symbol at its env field (its frame lives on the caller's stack). Reuse
  // that pre-set alloca storage instead of allocating a fresh non-persisting slot.
  const auto var_sym = sm->CurrentScope->GetVarSymbol(Alias != nullptr ? Alias->Name.get() : Name.get());

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
    meta->Save();
    meta->AssignmentTarget = Alias != nullptr ? Alias->Name : Name;
    meta->LlvmAssignmentTarget = alloca;

    // Fix variable shadowing, where a newer version of the
    // symbol is gotten because stage7 added it, when we are
    // trying to use the original.
    const auto sym_name = Alias != nullptr ? Alias->Name.get() : Name.get();
    const auto shadowed = sm->CurrentScope->RemVarSymbol(sym_name);

    auto llvm_val = meta->LetStatementValue->Stage11_CodeGen(sm, meta, ctx);
    if (shadowed != nullptr) { sm->CurrentScope->AddVarSymbol(shadowed); }

    // The declared type may be a variant that the initializer
    // is only a member of ("let x: Opt[S32] = Some(val=1)"),
    // in which case the value has to be tagged and copied into
    // the payload. Storing it raw put the member at offset zero,
    // on top of the tag, so the slot read back as whatever the
    // member's first bytes happened to be.
    if (not is_void and meta->LetStatementExplicitType != nullptr) {
      llvm_val = codegen::CoerceToVariant(
        llvm_val, *meta->LetStatementExplicitType, *meta->LetStatementValue->InferType(sm, meta),
        *sm->CurrentScope, "local.variant" + uid, ctx);
    }

    // Skip storing Void (created via generic implementation
    // analysis).
    if (not is_void) { ctx->Builder.CreateStore(llvm_val, alloca); }
    meta->Restore();
  }

  // Alloca already added; return nullptr.
  return nullptr;
}

auto spp::asts::LocalVariableSingleIdentifierAst::ExtractNames() const
  -> Vec<Shared<IdentifierAst>> {
  // Return the single name as a vector that can get appended to
  // from nesting.
  return {Name};
}

auto spp::asts::LocalVariableSingleIdentifierAst::ExtractName() const
  -> Shared<IdentifierAst> {
  // Return the single name (identifier) for matching capability.
  return Name;
}

SPP_MOD_END
