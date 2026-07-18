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
import spp.analyse.utils.visibility_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.strings;
import spp.utils.uid;
import ankerl;
import genex;
import llvm;

SPP_MOD_BEGIN
auto spp::asts::IdentifierAst::FromType(
    TypeAst const &val)
    -> Unique<IdentifierAst> {
    return MakeUnique<IdentifierAst>(val.PosStart(), Str(val.TypeParts().Back()->Name));
}

spp::asts::IdentifierAst::IdentifierAst(
    const std::size_t pos,
    decltype(Val) val) :
    Val(std::move(val)),
    _Pos(pos) {
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
    if (Val == other.Val) {
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
    return MakeUnique<IdentifierAst>(_Pos, Str(Val));
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

    // Check there is a symbol with the same name in the current scope.
    const auto shared = AstCloneShared(this);
    if (not sm->CurrentScope->HasVarSymbol(shared) and not sm->CurrentScope->HasNsSymbol(shared)) {
        RaiseIf<SppSelfIdentifierInvalidContextError>(Val == "self", {sm->CurrentScope}, ERR_ARGS(*this));
        RaiseMissingIdentifierAndClosestOptions(*this, sm->CurrentScope->AllVarSymbols(), {}, *sm);
    }

    // Enforce module-level visibility on the accessed symbol.
    if (const auto sym = sm->CurrentScope->GetVarSymbol(shared)) {
        if (sym->ScopeDefinedIn != nullptr and sym->ScopeDefinedIn->TySym == nullptr) {
            CheckModuleMemberVisibility(*sym, *this, *sym->ScopeDefinedIn, *sm, *meta);
        }
    }
}

auto spp::asts::IdentifierAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppCompileTimeConstantError;

    // Extract the value from the symbol table and return it.
    const auto var_sym = sm->CurrentScope->GetVarSymbol(AstClone(this));
    auto tm = analyse::scopes::ScopeManager(
        sm->GlobalScope, var_sym->ScopeDefinedIn ? : sm->CurrentScope);

    RaiseIf<SppCompileTimeConstantError>(
        var_sym != nullptr and var_sym->CompTimeValue == nullptr,
        {sm->CurrentScope}, ERR_ARGS(*this));

    var_sym->CompTimeValue->Stage9_CompTimeResolve(&tm, meta);
}

auto spp::asts::IdentifierAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::errors::SppInternalCompilerError;

    // Get the allocation for the variable from the current scope.
    const auto uid = spp::utils::Uid(this);
    const auto var_sym = sm->CurrentScope->GetVarSymbol(AstClone(this));
    SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);

    // Handle local variable allocation extraction + load.
    if (llvm::isa<llvm::AllocaInst>(var_sym->LlvmInfo->Alloca)) {
        const auto alloca = llvm::cast<llvm::AllocaInst>(var_sym->LlvmInfo->Alloca);
        return ctx->Builder.CreateLoad(alloca->getAllocatedType(), alloca, "load.local" + uid);
    }

    // Handle global variable (load from global).
    if (llvm::isa<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca)) {
        const auto global_var = llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca);
        return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.global" + uid);
    }

    // If the variable is neither local nor global, this is an internal compiler error.
    Raise<SppInternalCompilerError>(
        {sm->CurrentScope},
        ERR_ARGS(*this, "Target identifier ie neither local or global"));
}

auto spp::asts::IdentifierAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // Extract the symbol from the current scope, as a variable symbol.
    const auto var_sym = sm->CurrentScope->GetVarSymbol(AstClone(this));
    return var_sym ? var_sym->Type : nullptr;
}

auto spp::asts::IdentifierAst::ToFuncIdentifier() const
    -> Unique<IdentifierAst> {
    return MakeUnique<IdentifierAst>(_Pos, "$" + spp::utils::strings::SnakeToPascal(Val));
}

auto spp::asts::IdentifierAst::AnkerlHash() const
    -> std::size_t {
    return ankerl::unordered_dense::hash<Str>()(Val);
}

auto spp::asts::IdentifierAst::ExprParts() const
    -> Vec<Ast*> {
    return {const_cast<IdentifierAst*>(this)};
}

auto spp::asts::IdentifierAst::ToView() const noexcept
    -> StrView {
    return Val;
}

SPP_MOD_END
