module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.visibility_utils;
import spp.asts.expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_func;
import spp.lex.tokens;
import spp.utils.strings;
import spp.utils.uid;
import genex;

namespace {
  auto StaticMemberOf(
    spp::analyse::scopes::Scope &type_scope,
    spp::asts::IdentifierAst const &name)
    -> spp::analyse::scopes::VariableSymbol* {
    using spp::analyse::utils::expr_utils::LookupMemberForAccess;
    using spp::analyse::utils::expr_utils::MemberAccessForm;
    using spp::analyse::utils::expr_utils::MemberReachableBy;

    // If the scope symbol exists (nullptr check for type
    // forwarding), and the member can be runtime-accessed,
    // then return the found symbol.
    const auto found = type_scope.GetVarSymbol(&name, true);
    if (found == nullptr or MemberReachableBy(
      *found, MemberAccessForm::Static)) { return found; }

    // If the cheap check gave a runtime symbol, then
    // search more deeply through the super scopes to
    // find the member in a static context.
    const auto member = LookupMemberForAccess(
      type_scope, name, MemberAccessForm::Static);
    return member != nullptr ? member : found;
  }
}

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PostfixExpressionOperatorStaticMemberAccessAst(
  decltype(TokDblColon) &&tok_dbl_colon,
  decltype(Name) &&name) :
  TokDblColon(std::move(tok_dbl_colon)),
  Name(std::move(name)),
  _LhsTypeSym(nullptr) {
}

spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::~PostfixExpressionOperatorStaticMemberAccessAst() = default;

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PosStart() const
  -> std::size_t {
  // Use the "::" token.
  return Name->PosStart();
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PosEnd() const
  -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto p = MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
    AstClone(TokDblColon),
    AstClone(Name));
  p->_LhsTypeSym = _LhsTypeSym;
  return p;
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("::");
  SPP_STRING_APPEND(Name);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions;
  using analyse::utils::visibility_utils::CheckModuleMemberVisibility;
  using analyse::utils::visibility_utils::CheckTypeMemberVisibility;
  using analyse::utils::expr_utils::ClosestScopes;
  using analyse::utils::expr_utils::RaiseIfAmbiguous;
  using analyse::utils::expr_utils::LookupMemberForAccess;
  using analyse::utils::expr_utils::MemberAccessForm;
  using analyse::utils::expr_utils::MemberReachableBy;
  using analyse::utils::expr_utils::MembersReachableBy;
  using analyse::utils::expr_utils::ScopesDeclaringVar;
  using analyse::errors::SppMemberAccessRuntimeOperatorExpectedError;

  // Handle types on the left-hand-side of a static member access.
  if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_as_type);
    _LhsTypeSym = lhs_type_sym;

    // Check the target field exists on the type.
    if (not lhs_type_sym->LinkedScope->HasVarSymbol(Name.get(), true)) {
      auto [fwd_ref_type, fwd_mut_type] = analyse::utils::type_utils::GetFwdTypes(*lhs_type_sym->FqName(), *sm);
      const auto temp = fwd_ref_type ? fwd_ref_type->LastTypePart()->GnArgGroup->TypeAt("T")->Val.get() : nullptr;
      const auto lhs_fwd_ref_type_sym = temp ? sm->CurrentScope->GetTypeSymbol(temp) : nullptr;
      const auto found = lhs_fwd_ref_type_sym
        ? lhs_fwd_ref_type_sym->LinkedScope->HasVarSymbol(Name.get(), true)
        : false;
      if (fwd_ref_type == nullptr or (fwd_ref_type != nullptr and not found)) {
        // Todo: Need to filter these candidates to function groups who contain a static overload.
        // Todo: Add fwd-ref type member candidates

        auto candidates = lhs_type_sym->LinkedScope->AllVarSymbols(true, true)
          | genex::views::filter([](auto const &sym) { return sym->Type->IsCompilerGeneratedType(); })
          | genex::to<Vec>();
        RaiseMissingIdentifierAndClosestOptions(*Name, std::move(candidates), {}, *sm);
      }
      _LhsTypeSym = lhs_fwd_ref_type_sym;
    }

    // Check there is only 1 target field on the type at the highest level.
    if (_LhsTypeSym->LinkedScope->GetVarSymbol(Name.get(), true)->Type->IsCompilerGeneratedType()) {
      return;
    }

    // A class attribute belongs to a value of the type rather
    // than to the type, so it is reached with "." instead.
    const auto found = _LhsTypeSym->LinkedScope->GetVarSymbol(Name.get(), true);
    if (found != nullptr and not MemberReachableBy(*found, MemberAccessForm::Static)) {
      const auto member = LookupMemberForAccess(
        *_LhsTypeSym->LinkedScope, *Name, MemberAccessForm::Static);

      RaiseIf<SppMemberAccessRuntimeOperatorExpectedError>(
        member == nullptr, {sm->CurrentScope},
        ERR_ARGS(*Name, *TokDblColon, "attribute"));
    }

    // Which declaration the access resolves to: the nearest
    // scope that can reach the name, supers included. Only
    // the members "::" reaches are in the running, so a class
    // attribute of the same name neither answers this nor makes
    // it ambiguous.
    const auto closest = ClosestScopes(MembersReachableBy(
      ScopesDeclaringVar(*_LhsTypeSym->LinkedScope, *Name, true), MemberAccessForm::Static));

    // Enforce visibility on the accessed member. Visibility is
    // read off the non-generic scope, because that is where the
    // member was written and so where its annotation lives; an
    // instantiation's copy of a symbol is not the declaration.
    if (not closest.IsEmpty()) {
      const auto scope = closest[0].Where->NonGenericScope;
      const auto declared_sym = scope->GetVarSymbol(Name.get());
      CheckTypeMemberVisibility(
        declared_sym != nullptr ? *declared_sym : *closest[0].Symbol, *Name,
        declared_sym != nullptr ? *scope : *closest[0].Where, *sm, *meta);
    }

    RaiseIfAmbiguous(
      ClosestScopes(
        MembersReachableBy(
          ScopesDeclaringVar(*_LhsTypeSym->LinkedScope, *Name, false), MemberAccessForm::Static)), *Name,
      *sm);
    return;
  }

  // Otherwise, we are handling a namespace left-hand-side.
  const auto lhs_as_ident = meta->PostfixExpressionLhs->To<IdentifierAst>();
  const auto lhs_var_sym = sm->CurrentScope->GetVarSymbol(lhs_as_ident);

  // Check the lhs is a namespace and not a variable.
  RaiseIf<SppMemberAccessRuntimeOperatorExpectedError>(
    lhs_var_sym != nullptr, {sm->CurrentScope},
    ERR_ARGS(*meta->PostfixExpressionLhs, *TokDblColon, "variable"));

  // Check the constant exists inside the namespace.
  // Todo: inconsistent "true" for exclusive here vs ns
  const auto lhs_ns_sym = sm->CurrentScope->ConvertPostfixToNestedScope(meta->PostfixExpressionLhs)->NsSym;
  if (not lhs_ns_sym->LinkedScope->HasVarSymbol(Name.get(), true) and not lhs_ns_sym->LinkedScope->HasNsSymbol(
    Name.get(), true)) {
    RaiseMissingIdentifierAndClosestOptions(
      *Name, lhs_ns_sym->LinkedScope->AllVarSymbols(false, true), lhs_ns_sym->LinkedScope->AllNsSymbols(), *sm);
  }

  // Enforce visibility on the accessed namespace symbol.
  // Only for var symbols, not namespace symbols.
  if (const auto sym = lhs_ns_sym->LinkedScope->GetVarSymbol(Name.get())) {
    CheckModuleMemberVisibility(*sym, *Name, *lhs_ns_sym->LinkedScope, *sm, *meta);
  }
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Handle accessing a symbol on a type.
  if (_LhsTypeSym != nullptr) {
    const auto sym = StaticMemberOf(*_LhsTypeSym->LinkedScope, *Name);
    auto tm = ScopeManager(sm->GlobalScope, _LhsTypeSym->LinkedScope);
    sym->CompTimeValue->Stage9_CompTimeResolve(&tm, meta);
    meta->CmpResult = AstClone(meta->CmpResult);
    return;
  }

  // Handle accessing a variable on a namespace.
  // Todo: Do we need to call stage_9 on the value?
  const auto lhs = meta->PostfixExpressionLhs;
  const auto lhs_ns_sym = sm->CurrentScope->ConvertPostfixToNestedScope(lhs)->NsSym;
  const auto sym = lhs_ns_sym->LinkedScope->GetVarSymbol(Name.get(), true);
  meta->CmpResult = AstClone(sym->CompTimeValue->To<ExpressionAst>());
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  const auto uid = "." + spp::utils::Uid(this);

  // In a constant context the caller wants a value,
  // not a load. Resolve recursively and return the
  // result from the meta context.
  if (ctx->InConstantContext) {
    Stage9_CompTimeResolve(sm, meta);
    if (const auto folded = std::move(meta->CmpResult); folded != nullptr) {
      return folded->Stage11_CodeGen(sm, meta, ctx);
    }
  }

  // Type case: LHS is a TypeAst — access a cmp constant on the type's scope.
  if (_LhsTypeSym != nullptr) {
    const auto var_sym = StaticMemberOf(*_LhsTypeSym->LinkedScope, *Name);
    if (var_sym->Type->IsCompilerGeneratedType()) { return nullptr; }
    SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);
    const auto global_var = codegen::GetOrAddGlobalIntoCurrentModule(
      *llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca),
      *codegen::GetEmissionModule(*ctx));
    return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.static_type_member" + uid);
  }

  // Namespace case: LHS is a namespace identifier — access a cmp constant in the namespace's scope.
  const auto lhs_ns_scope = sm->CurrentScope->ConvertPostfixToNestedScope(meta->PostfixExpressionLhs);
  const auto var_sym = lhs_ns_scope->GetVarSymbol(Name.get(), true);
  if (var_sym->Type->IsCompilerGeneratedType()) { return nullptr; }
  SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);
  const auto global_var = codegen::GetOrAddGlobalIntoCurrentModule(
    *llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca),
    *codegen::GetEmissionModule(*ctx));
  return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.static_ns_member" + uid);
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  //
  using analyse::utils::type_utils::GetFwdTypes;
  // Todo: use the stored symbol? that if it's null? is that possible ie if used before analysis? shouldn't be.

  // Get the left-hand-side type's member's type.
  if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
    // todo: const auto sym = _LhsTypeSym->LinkedScope->GetVarSymbol(Name.get(), true);
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_as_type);
    const auto sym = StaticMemberOf(*lhs_type_sym->LinkedScope, *Name);
    if (sym != nullptr) { return sym->Type; }

    // This is where we need to handle the FwdRef/FwdMut logic.
    auto [fwd_ref_type, _] = GetFwdTypes(*lhs_as_type, *sm);
    const auto inner_type = fwd_ref_type->LastTypePart()->GnArgGroup->TypeAt("T")->Val;
    const auto inner_type_sym = sm->CurrentScope->GetTypeSymbol(inner_type.get());
    const auto fwd_sym = inner_type_sym->LinkedScope->GetVarSymbol(Name.get(), true);
    return fwd_sym->Type;
  }

  // Get the left-hand-side namespace's member's type.
  const auto lhs_ns_scope = sm->CurrentScope->ConvertPostfixToNestedScope(meta->PostfixExpressionLhs);
  const auto type = lhs_ns_scope->GetVarSymbol(Name.get(), true)->Type;
  return lhs_ns_scope->GetTypeSymbol(type.get())->FqName();
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::ExprParts() const
  -> Vec<IdentifierAst*> {
  // Static member access does not have any expression parts.
  return {Name.get()};
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::IsAllowedInDefault() const
  -> bool {
  // Reads what it is applied to, and holds nothing of its own.
  return true;
}

SPP_MOD_END
