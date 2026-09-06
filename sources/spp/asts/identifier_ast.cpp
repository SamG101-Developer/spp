module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.identifier_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.visibility_utils;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_func;
import spp.codegen.llvm_type;
import spp.utils.strings;
import spp.utils.uid;
import genex;
import llvm;

namespace {
  /**
   * The symbols whose compile-time values are currently being resolved. A "cmp" constant is resolved by walking the
   * identifiers its value names, so a constant that reaches itself is a cycle and therefore an error.
   */
  thread_local spp::Vec<spp::analyse::scopes::VariableSymbol const*> _ResolvingCompTimeSyms;

  /**
   * Push a symbol onto the resolution stack for as long as the enclosing block runs, including when it is left by a
   * thrown semantic error - a caught error must not leave the stack claiming a resolution is still in progress.
   */
  struct ResolvingCompTimeSymGuard {
    explicit ResolvingCompTimeSymGuard(
      spp::analyse::scopes::VariableSymbol const *const sym) { _ResolvingCompTimeSyms.EmplaceBack(sym); }

    ~ResolvingCompTimeSymGuard() { _ResolvingCompTimeSyms.PopBack(); }

    ResolvingCompTimeSymGuard(ResolvingCompTimeSymGuard const&) = delete;
    ResolvingCompTimeSymGuard(ResolvingCompTimeSymGuard&&) = delete;
    auto operator=(ResolvingCompTimeSymGuard const&) -> ResolvingCompTimeSymGuard& = delete;
    auto operator=(ResolvingCompTimeSymGuard&&) -> ResolvingCompTimeSymGuard& = delete;
  };
}

SPP_MOD_BEGIN
auto spp::asts::IdentifierAst::FromType(
  TypeAst const &val)
  -> Unique<IdentifierAst> {
  return MakeUnique<IdentifierAst>(val.PosStart(), Str(val.LastTypePart()->Name));
}

spp::asts::IdentifierAst::IdentifierAst(
  const std::size_t pos,
  decltype(Val) val) :
  Val(std::move(val)),
  _Pos(pos),
  _NameId(utils::Intern(Val)) {
}

spp::asts::IdentifierAst::IdentifierAst(
  const std::size_t pos,
  decltype(Val) val,
  const utils::InternedId name_id) :
  Val(std::move(val)),
  _Pos(pos),
  _NameId(name_id) {
}

auto spp::asts::IdentifierAst::MappedFromTok(
  TokenAst const &tok,
  decltype(Val) val)
  -> Unique<IdentifierAst> {
  //
  auto id = MakeUnique<IdentifierAst>(tok.PosStart(), std::move(val));
  id->_ForTok = tok.TokenData.length();
  return id;
}

spp::asts::IdentifierAst::~IdentifierAst() = default;

auto spp::asts::IdentifierAst::operator<=>(
  IdentifierAst const &that) const
  -> Ordering {
  return Val <=> that.Val;
}

auto spp::asts::IdentifierAst::operator==(
  IdentifierAst const &that) const
  -> bool {
  return EqualsIdentifier(that) == Ordering::equal;
}

auto spp::asts::IdentifierAst::operator==(
  ExpressionAst const &that) const
  -> bool {
  return Equals(that) == Ordering::equal;
}

auto spp::asts::IdentifierAst::EqualsIdentifier(
  IdentifierAst const &other) const
  -> Ordering {
  if (_NameId == other._NameId) {
    return Ordering::equal;
  }
  return Ordering::less;
}

auto spp::asts::IdentifierAst::Equals(
  ExpressionAst const &other) const
  -> Ordering {
  return other.EqualsIdentifier(*this);
}

auto spp::asts::IdentifierAst::PosStart() const
  -> std::size_t {
  return _Pos;
}

auto spp::asts::IdentifierAst::PosEnd() const
  -> std::size_t {
  return _ForTok ? _Pos + _ForTok : _Pos + Val.length();
}

auto spp::asts::IdentifierAst::Clone() const
  -> Unique<Ast> {
  // The copy spells the same name, so it carries the
  // id over rather than interning the string again.
  return Unique<IdentifierAst>(new IdentifierAst(_Pos, Str(Val), _NameId));
}

auto spp::asts::IdentifierAst::ToString() const
  -> Str {
  return Val;
}

auto spp::asts::IdentifierAst::operator+(
  IdentifierAst const &that) const
  -> IdentifierAst {
  return IdentifierAst(_Pos, Val + that.Val);
}

auto spp::asts::IdentifierAst::operator+(
  Str const &that) const
  -> IdentifierAst {
  return IdentifierAst(_Pos, Val + that);
}

auto spp::asts::IdentifierAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppSelfIdentifierInvalidContextError;
  using analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions;
  using analyse::utils::visibility_utils::CheckModuleMemberVisibility;

  // Check there is a symbol with the same name in the
  // current scope. Also check for invalid "self" (just
  // a custom error for "self" in a non-method context).
  const auto sym = sm->CurrentScope->GetVarSymbol(this);
  if (sym == nullptr and not sm->CurrentScope->HasNsSymbol(this)) {
    RaiseIf<SppSelfIdentifierInvalidContextError>(Val == "self", {sm->CurrentScope}, ERR_ARGS(*this));
    RaiseMissingIdentifierAndClosestOptions(*this, sm->CurrentScope->AllVarSymbols(), {}, *sm);
  }

  // Enforce module-level visibility on the accessed symbol.
  if (sym != nullptr and sym->ScopeDefinedIn != nullptr and sym->ScopeDefinedIn->TySym == nullptr) {
    CheckModuleMemberVisibility(*sym, *this, *sym->ScopeDefinedIn, *sm, *meta);
  }
}

auto spp::asts::IdentifierAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppCompileTimeConstantError;

  // Extract the value from the symbol table and return
  // it.
  const auto var_sym = sm->CurrentScope->GetVarSymbol(this);
  auto tm = analyse::scopes::ScopeManager(
    sm->GlobalScope, var_sym->ScopeDefinedIn ? : sm->CurrentScope);

  // If there is no comptime value on this symbol, then
  // it's an error (not sure this ever triggers? - a non
  // cmp function call triggers an error i think).
  RaiseIf<SppCompileTimeConstantError>(
    var_sym != nullptr and var_sym->CompTimeValue == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*this));

  // A constant whose value reaches its own symbol again has
  // no value to resolve to: the walk below would re-enter
  // here for the same symbol and never terminate.
  RaiseIf<SppCompileTimeConstantError>(
    genex::contains(_ResolvingCompTimeSyms, var_sym),
    {sm->CurrentScope}, ERR_ARGS(*this));

  // Call the inner resolution on the provided value for
  // "walking" the comptime resolution.
  const auto guard = ResolvingCompTimeSymGuard(var_sym);
  var_sym->CompTimeValue->Stage9_CompTimeResolve(&tm, meta);
}

auto spp::asts::IdentifierAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  //
  using analyse::errors::SppInternalCompilerError;
  using analyse::utils::type_predicates::IsTypeVoid;

  // Get the allocation for the variable from the current
  // scope. The "alloca" will have been filled from wherever
  // this identifier was introduced ("let", param, etc).
  const auto uid = "." + spp::utils::Uid(this);
  const auto var_sym = sm->CurrentScope->GetVarSymbol(this);

  // An identifier that reaches code generation with no symbol
  // behind it is an internal error. Report it as one, naming the
  // identifier, rather than reading through the null pointer -
  // the same way the missing-allocation check below does. Todo:
  // This *can* trigger when a symbol is used on the left and right
  // like "let x = x.something()" when rebinding symbol names.
  RaiseIf<SppInternalCompilerError>(
    var_sym == nullptr, {sm->CurrentScope},
    ERR_ARGS(*this, "Target identifier has no symbol"));

  // Void identifiers could be created via generic
  // implementation, to prevent any usages of it as this
  // level too.
  if (var_sym->Type != nullptr and IsTypeVoid(*var_sym->Type, *sm->CurrentScope)) {
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
    const auto llvm_type = codegen::GetLlvmTypeOf(*var_sym->Type, *sm->CurrentScope, ctx);
    SPP_ASSERT(llvm_type != nullptr);
    return ctx->Builder.CreateLoad(llvm_type, var_sym->LlvmInfo->Alloca, "load.flow" + uid);
  }

  // If the variable is neither local nor global, this is an
  // internal compiler error.
  Raise<SppInternalCompilerError>(
    {sm->CurrentScope},
    ERR_ARGS(*this, "Target identifier ie neither local nor global"));
}

auto spp::asts::IdentifierAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *)
  -> Shared<TypeAst> {
  // Extract the symbol from the current scope, as a variable
  // symbol.
  const auto var_sym = sm->CurrentScope->GetVarSymbol(this);
  return var_sym ? var_sym->Type : nullptr;
}

auto spp::asts::IdentifierAst::ToFuncIdentifier() const
  -> Unique<IdentifierAst> {
  // Convert the identifier into pascal case and wrap it with
  // the compiler-type "$" token; for example, "func_name"
  // becomes "$FuncName".
  return MakeUnique<IdentifierAst>(
    _Pos, "$" + spp::utils::strings::SnakeToPascal(Val));
}

auto spp::asts::IdentifierAst::AnkerlHash() const
  -> std::size_t {
  // Consistent with "EqualsIdentifier", which decides
  // equality on the id, and a multiply rather than a
  // pass over the string.
  return Hash<utils::InternedId>()(_NameId);
}

auto spp::asts::IdentifierAst::ExprParts() const
  -> Vec<Ast*> {
  return {const_cast<IdentifierAst*>(this)};
}

auto spp::asts::IdentifierAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &args) const
  -> Shared<ExpressionAst> {
  // A comp parameter's name is written as a type in the
  // parameter list and read as an identifier in an
  // expression, so the two spellings have to be brought
  // together before they can be compared.
  for (auto const *arg : args) {
    auto const *comp_kw_arg = arg->To<GenericArgumentCompKeywordAst>();
    if (comp_kw_arg == nullptr) { continue; }
    if (*FromType(*comp_kw_arg->Name) != *this) { continue; }
    return AstCloneShared(comp_kw_arg->Val.get());
  }

  // Any other identifier names something the bindings
  // say nothing about - a local, a constant, a function.
  return AstCloneShared(this);
}

auto spp::asts::IdentifierAst::ToView() const noexcept
  -> StrView {
  return Val;
}

SPP_MOD_END
