module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.identifier_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.analyse.utils.visibility_utils;
import spp.asts.generic_argument_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_fn;
import spp.codegen.llvm_type;
import spp.lsp.resolution_index;
import spp.utils.interner;
import spp.utils.strings;
import spp.utils.uid;
import genex;
import llvm;

namespace {
  /// List of the symbols those comp-time values are in the
  /// process of being resolved. A "cmp" constant is resolved by
  /// walking the identifiers its value names, so a constant
  /// that reaches itself is a cycle => error.
  thread_local spp::Vec<VariableSymbol const*> _ResolvingCompTimeSymbols;

  /// Push a symbol onto the resolution stack for as long as
  /// the enclosing block runs, including when it is left by a
  /// thrown semantic error. A caught error must not leave the
  /// stack claiming a resolution is still in progress.
  struct ResolvingCompTimeSymbolGuard {
    explicit ResolvingCompTimeSymbolGuard(
      VariableSymbol const *const sym) { _ResolvingCompTimeSymbols.EmplaceBack(sym); }

    ~ResolvingCompTimeSymbolGuard() { _ResolvingCompTimeSymbols.PopBack(); }

    ResolvingCompTimeSymbolGuard(ResolvingCompTimeSymbolGuard const &) = delete;
    ResolvingCompTimeSymbolGuard(ResolvingCompTimeSymbolGuard &&) = delete;
    auto operator=(ResolvingCompTimeSymbolGuard const &) -> ResolvingCompTimeSymbolGuard& = delete;
    auto operator=(ResolvingCompTimeSymbolGuard &&) -> ResolvingCompTimeSymbolGuard& = delete;
  };
}

SPP_MOD_BEGIN
auto IdentifierAst::FromType(
  TypeAst const &val) -> Unique<IdentifierAst> {
  return MakeUnique<IdentifierAst>(val.PosStart(), Str(val.LastTypePart()->Name));
}

IdentifierAst::IdentifierAst(
  const std::size_t pos,
  decltype(Val) val) :
  Val(std::move(val)),
  _Pos(pos),
  _NameId(utils::Intern(Val)) {
}

IdentifierAst::IdentifierAst(
  const std::size_t pos,
  decltype(Val) val,
  const utils::InternedId name_id) :
  Val(std::move(val)),
  _Pos(pos),
  _NameId(name_id) {
}

auto IdentifierAst::MappedFromTok(
  TokenAst const &tok, decltype(Val) val) -> Unique<IdentifierAst> {
  //
  // As many raw tokens as the token covers, which is not always
  // its length: a keyword ("self", "and") is one raw token.
  auto id = MakeUnique<IdentifierAst>(tok.PosStart(), std::move(val));
  id->_ForTok = tok.PosEnd() - tok.PosStart();
  return id;
}

IdentifierAst::~IdentifierAst() = default;

auto IdentifierAst::operator<=>(
  IdentifierAst const &that) const -> Ordering {
  return Val <=> that.Val;
}

auto IdentifierAst::operator==(
  IdentifierAst const &that) const -> bool {
  return EqualsIdentifier(that) == Ordering::equal;
}

auto IdentifierAst::operator==(
  ExpressionAst const &that) const -> bool {
  return Equals(that) == Ordering::equal;
}

auto IdentifierAst::EqualsIdentifier(
  IdentifierAst const &other) const -> Ordering {
  // Compare the interned identifiers.
  return _NameId == other._NameId
    ? Ordering::equal
    : Ordering::less;
}

auto IdentifierAst::Equals(
  ExpressionAst const &other) const -> Ordering {
  // Into reverse hook.
  return other.EqualsIdentifier(*this);
}

auto IdentifierAst::PosStart() const -> std::size_t {
  // Raw position field.
  return _Pos;
}

auto IdentifierAst::PosEnd() const -> std::size_t {
  // Raw position field incremented by the token data length.
  return _ForTok ? _Pos + _ForTok : _Pos + Val.length();
}

auto IdentifierAst::Clone() const -> Unique<Ast> {
  // The copy spells the same name, so it carries the
  // id over rather than interning the string again. A
  // name mapped from a token keeps that token's length.
  auto id = Unique<IdentifierAst>(new IdentifierAst(_Pos, Str(Val), _NameId));
  id->_ForTok = _ForTok;
  id->_StampedCompId = _StampedCompId;
  return id;
}

auto IdentifierAst::ToString() const -> Str {
  // Just use the internal value.
  return Val;
}

auto IdentifierAst::operator+(
  IdentifierAst const &that) const -> IdentifierAst {
  // Append another identifier into this one.
  return IdentifierAst(_Pos, Val + that.Val);
}

auto IdentifierAst::operator+(
  Str const &that) const -> IdentifierAst {
  // Append a raw string into this one.
  return IdentifierAst(_Pos, Val + that);
}

auto IdentifierAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Check there is a symbol with the same name in the
  // current scope. Also check for invalid "self" (just
  // a custom error for "self" in a non-method context).
  const auto sym = sm->CurrentScope->FindVarSymbol(this);

  // A comp parameter (or a binding of one) named here is recorded, as a type parameter's name is: wherever the name
  // is read from then, it means this parameter ("Scope::FindBoundVarSymbolById"), not whatever its spelling finds
  // there.
  if (sym != nullptr) { type_resolution::StampComp(*this, *sym); }

  if (sym == nullptr and not sm->CurrentScope->HasNsSymbol(this)) {
    RaiseIf<SppSelfIdentifierInvalidContextError>(Val == "self", {sm->CurrentScope}, ERR_ARGS(*this));
    member_lookup::RaiseMissingIdentifierAndClosestOptions(*this, sm->CurrentScope->GetAllVarSymbols(), {}, *sm);
  }

  // A binding from an "is" read where the match is not known to
  // have succeeded (after the statement, in an "else", on the
  // right of an "or") names nothing there.
  if (sym != nullptr and genex::contains(meta->ExpiredIsBindings, sym)) {
    member_lookup::RaiseMissingIdentifierAndClosestOptions(*this, {}, {}, *sm);
  }

  // A parameter's default is analysed where the parameters are
  // in scope, but it is copied into the calls, where this name
  // would mean whatever the caller has under it.
  RaiseIf<SppDefaultValueNamesParameterError>(
    meta->ParameterDefaultScope != nullptr and sym != nullptr and sym->Kind == VariableKind::Local
    and meta->ParameterDefaultScope->HasVarSymbol(this, true),
    {sm->CurrentScope}, ERR_ARGS(*this));

  // Enforce module-level visibility on the accessed symbol.
  if (sym != nullptr and sym->ScopeDefinedIn != nullptr and sym->ScopeDefinedIn->LinkedTypeSymbol == nullptr) {
    visibility_utils::CheckModuleMemberVisibility(*sym, *this, *sym->ScopeDefinedIn, *sm, *meta);
  }

  // Use the hook to record information for the resolution and
  // completion plugin.
  lsp::resolution_index::RecordIdentifier(*this, *sm, *meta, sym);
}

auto IdentifierAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Extract the value from the symbol table and return
  // it.
  const auto var_sym = sm->CurrentScope->FindVarSymbol(this);
  auto tm = ScopeManager(
    sm->GlobalScope, var_sym->ScopeDefinedIn ? var_sym->ScopeDefinedIn : sm->CurrentScope);

  // An unbound comp generic has no value yet, and stands for
  // itself - as it does in a template's signature.
  if (var_sym != nullptr and var_sym->Kind == VariableKind::GnCompParam) {
    meta->CompTimeResult = AstClone(this);
    return;
  }

  // Anything else resolves through its value, and having none
  // is an error.
  auto *const value = var_sym != nullptr ? var_sym->CompTimeValue.get() : nullptr;
  RaiseIf<SppCompTimeConstantError>(
    var_sym != nullptr and value == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*this));

  // A constant whose value reaches its own symbol again has
  // no value to resolve to: the walk below would re-enter
  // here for the same symbol and never terminate.
  RaiseIf<SppCompTimeConstantError>(
    genex::contains(_ResolvingCompTimeSymbols, var_sym),
    {sm->CurrentScope}, ERR_ARGS(*this));

  // Call the inner resolution on the provided value for
  // "walking" the comptime resolution.
  const auto guard = ResolvingCompTimeSymbolGuard(var_sym);
  value->Stage9_CompTimeResolve(&tm, meta);
}

auto IdentifierAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;

  // Get the allocation for the variable from the current
  // scope. The "alloca" will have been filled from wherever
  // this identifier was introduced ("let", param, etc).
  const auto uid = "." + Uid();
  const auto var_sym = sm->CurrentScope->FindVarSymbol(this);

  // An identifier that reaches code generation with no symbol
  // behind it is an internal error. Report it as one, naming
  // the identifier, rather than reading through the null pointer
  // - the same way the missing-allocation check below does. Todo:
  // This *can* trigger when a symbol is used on the left and right
  // like "let x = x.something()" when rebinding symbol names.
  RaiseIf<SppInternalCompilerError>(
    var_sym == nullptr, {sm->CurrentScope},
    ERR_ARGS(*this, "Target identifier has no symbol"));

  // Void identifiers could be created via generic
  // implementation, to prevent any usages of it as this
  // level too.
  if (var_sym->Type != nullptr
    and type_predicates::IsTypeVoid(var_sym->TypeRefIn(*sm->CurrentScope), *sm->CurrentScope)) {
    return nullptr;
  }

  // A symbol with no address by this point is an internal
  // error. Report it as one rather than asserting, so the
  // failure names the identifier.
  RaiseIf<SppInternalCompilerError>(
    var_sym->LlvmInfo->Alloca == nullptr, {sm->CurrentScope},
    ERR_ARGS(*this, "Target identifier has no allocation"));

  // Handle local variable allocation extraction + load.
  // This is from normal "let" statements via their local
  // variable asts.
  if (llvm::isa<llvm::AllocaInst>(var_sym->LlvmInfo->Alloca)) {
    const auto alloca = llvm::cast<llvm::AllocaInst>(var_sym->LlvmInfo->Alloca);
    return ctx->Builder.CreateLoad(alloca->getAllocatedType(), alloca, "load.local" + uid);
  }

  // Handle global constants. These values aren't created
  // with the normal stack alloca instruction, rather with
  // llvm's "ConstantXXX" method, wrapped into a global.
  if (llvm::isa<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca)) {
    const auto defined_global = llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca);
    const auto global_var = codegen::GetOrAddGlobalIntoCurrentModule(
      *defined_global, *codegen::GetEmissionModule(*ctx));
    return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.global" + uid);
  }

  // Handle any other address the symbol was pointed at, such
  // as the payload a variant's flow-typed symbol is narrowed
  // onto by a case pattern. These carry no llvm type of its
  // own under opaque pointers, so the load goes through the
  // symbol's own type instead of through the instruction that
  // produced the address.
  if (var_sym->LlvmInfo->Alloca->getType()->isPointerTy()) {
    const auto llvm_type = codegen::GetLlvmTypeOf(var_sym->TypeRefIn(*sm->CurrentScope), ctx);
    SPP_ASSERT(llvm_type != nullptr);
    return ctx->Builder.CreateLoad(llvm_type, var_sym->LlvmInfo->Alloca, "load.flow" + uid);
  }

  // If the variable is neither local nor global, this is an
  // internal compiler error.
  Raise<SppInternalCompilerError>(
    {sm->CurrentScope},
    ERR_ARGS(*this, "Target identifier ie neither local nor global"));
}

auto IdentifierAst::InferType(
  ScopeManager *sm, CompilerMetaData *) -> Shared<TypeAst> {
  // Extract the symbol from the current scope, as a variable
  // symbol.
  const auto var_sym = sm->CurrentScope->FindVarSymbol(this);
  return var_sym ? var_sym->Type : nullptr;
}

auto IdentifierAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  // The variable's type, resolved here as its "InferType"
  // would be read.
  const auto var_sym = sm->CurrentScope->FindVarSymbol(this);
  return var_sym ? var_sym->TypeRefIn(*sm->CurrentScope) : TypeRef{};
}

auto IdentifierAst::ToFnIdentifier() const -> Unique<IdentifierAst> {
  // Convert the identifier into pascal case and wrap it with
  // the compiler-type "$" token; for example, "func_name"
  // becomes "$FuncName".
  return MakeUnique<IdentifierAst>(
    _Pos, "$" + spp::utils::strings::SnakeToPascal(Val));
}

auto IdentifierAst::AnkerlHash() const -> std::size_t {
  // Consistent with "EqualsIdentifier", which decides
  // equality on the id, and a multiply rather than a
  // pass over the string.
  return Hash<utils::InternedId>()(_NameId);
}

auto IdentifierAst::ExprParts() const -> Vec<IdentifierAst*> {
  return {const_cast<IdentifierAst*>(this)};
}

auto IdentifierAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Shared<ExpressionAst> {
  // A comp parameter (or a binding of one) is read by its identity where it was written, with the bindings applied
  // ("type_resolution::ReadComp"). Any other name - a local, a constant, a function - is opaque, and kept.
  return analyse::utils::type_resolution::ReadComp(*this, sub);
}

auto IdentifierAst::ToView() const noexcept -> StrView {
  return Val;
}

auto IdentifierAst::IsAllowedInDefault() const -> bool {
  // A name reads a value.
  return true;
}

SPP_MOD_END
