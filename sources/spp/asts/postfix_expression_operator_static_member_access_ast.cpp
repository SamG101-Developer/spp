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
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_resolution;
import spp.analyse.utils.visibility_utils;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_ast;
import spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_func;
import spp.lex.tokens;
import spp.lsp.resolution_index;
import spp.utils.ptr;
import spp.utils.strings;
import spp.utils.uid;
import genex;

namespace {
  auto StaticMemberOf(
    Scope &type_scope,
    IdentifierAst const &name)
    -> VariableSymbol* {
    IMPORT_UTILS;
    using member_lookup::MemberAccessForm;

    // If the scope symbol exists (nullptr check for type
    // forwarding), and the member can be runtime-accessed,
    // then return the found symbol.
    const auto found = type_scope.GetVarSymbol(&name, true);
    if (found == nullptr or member_lookup::MemberReachableBy(
      *found, MemberAccessForm::Static)) { return found; }

    // If the cheap check gave a runtime symbol, then
    // search more deeply through the super scopes to
    // find the member in a static context.
    const auto member = member_lookup::LookupMemberForAccess(
      type_scope, name, MemberAccessForm::Static);
    return member != nullptr ? member : found;
  }
}

SPP_MOD_BEGIN
PostfixExpressionOperatorStaticMemberAccessAst::PostfixExpressionOperatorStaticMemberAccessAst(
  decltype(TokDblColon) &&tok_dbl_colon,
  decltype(Name) &&name) :
  TokDblColon(std::move(tok_dbl_colon)),
  Name(std::move(name)),
  _LhsTypeSym(nullptr),
  _LhsNsScope(nullptr) {
}

PostfixExpressionOperatorStaticMemberAccessAst::~PostfixExpressionOperatorStaticMemberAccessAst() = default;

auto PostfixExpressionOperatorStaticMemberAccessAst::PosStart() const -> std::size_t {
  // Use the "::" token.
  return Name->PosStart();
}

auto PostfixExpressionOperatorStaticMemberAccessAst::PosEnd() const -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto PostfixExpressionOperatorStaticMemberAccessAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto p = MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
    AstClone(TokDblColon),
    AstClone(Name));
  p->_LhsTypeSym = _LhsTypeSym;
  p->_LhsNsScope = _LhsNsScope;
  return p;
}

auto PostfixExpressionOperatorStaticMemberAccessAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("::");
  SPP_STRING_APPEND(Name);
  SPP_STRING_END;
}

auto PostfixExpressionOperatorStaticMemberAccessAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;
  using member_lookup::MemberAccessForm;

  // Handle types on the left-hand-side of a static member access.
  if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_as_type);
    _LhsTypeSym = lhs_type_sym;

    // Check the target field exists on the type.
    if (not lhs_type_sym->LinkedScope->HasVarSymbol(Name.get(), true)) {
      const auto [fwd_ref_sym, _] = marker_sups::GetFwdTypes(*lhs_type_sym, *sm->CurrentScope);
      const auto lhs_fwd_ref_type_sym = fwd_ref_sym != nullptr ? fwd_ref_sym->BoundTypeArg("T") : nullptr;
      const auto found = lhs_fwd_ref_type_sym
        ? lhs_fwd_ref_type_sym->LinkedScope->HasVarSymbol(Name.get(), true)
        : false;
      if (fwd_ref_sym == nullptr or not found) {
        // Todo: Need to filter these candidates to function groups who contain a static overload.
        // Todo: Add fwd-ref type member candidates

        auto candidates = lhs_type_sym->LinkedScope->AllVarSymbols(true, true)
          | genex::views::filter([](auto const &sym) { return sym->Kind == VariableKind::Function; })
          | genex::to<Vec>();
        member_lookup::RaiseMissingIdentifierAndClosestOptions(*Name, std::move(candidates), {}, *sm);
      }
      _LhsTypeSym = lhs_fwd_ref_type_sym;
    }

    // Check there is only 1 target field on the type at the highest level.
    if (const auto named = _LhsTypeSym->LinkedScope->GetVarSymbol(Name.get(), true);
      named->Kind == VariableKind::Function) {
      // Use the hook to record information for the resolution and
      // completion plugin.
      lsp::resolution_index::RecordVariable(*Name, *sm, *meta, named);
      return;
    }

    // A class attribute belongs to a value of the type rather
    // than to the type, so it is reached with "." instead.
    const auto found = _LhsTypeSym->LinkedScope->GetVarSymbol(Name.get(), true);
    if (found != nullptr and not member_lookup::MemberReachableBy(*found, MemberAccessForm::Static)) {
      const auto member = member_lookup::LookupMemberForAccess(
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
    const auto closest = member_lookup::ClosestScopes(member_lookup::MembersReachableBy(
      member_lookup::ScopesDeclaringVar(*_LhsTypeSym->LinkedScope, *Name, true), MemberAccessForm::Static));

    // Enforce visibility on the accessed member. Visibility is
    // read off the non-generic scope, because that is where the
    // member was written and so where its annotation lives; an
    // instantiation's copy of a symbol is not the declaration.
    if (not closest.IsEmpty()) {
      const auto scope = closest[0].Where->NonGenericScope;
      const auto declared_sym = scope->GetVarSymbol(Name.get());
      visibility_utils::CheckTypeMemberVisibility(
        declared_sym != nullptr ? *declared_sym : *closest[0].Symbol, *Name,
        declared_sym != nullptr ? *scope : *closest[0].Where, *sm, *meta);

      // Use the hook to record information for the resolution and
      // completion plugin.
      lsp::resolution_index::RecordVariable(
        *Name, *sm, *meta, declared_sym != nullptr ? declared_sym : closest[0].Symbol);
    }

    member_lookup::RaiseIfAmbiguous(
      member_lookup::ClosestScopes(
        member_lookup::MembersReachableBy(
          member_lookup::ScopesDeclaringVar(
            *_LhsTypeSym->LinkedScope, *Name, false), MemberAccessForm::Static)), *Name,
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
  const auto lhs_ns_sym = LhsNsScope(sm, meta)->NsSym;
  if (not lhs_ns_sym->LinkedScope->HasVarSymbol(Name.get(), true) and not lhs_ns_sym->LinkedScope->HasNsSymbol(
    Name.get(), true)) {
    member_lookup::RaiseMissingIdentifierAndClosestOptions(
      *Name, lhs_ns_sym->LinkedScope->AllVarSymbols(false, true), lhs_ns_sym->LinkedScope->AllNsSymbols(), *sm);
  }

  // Enforce visibility on the accessed namespace symbol.
  // Only for var symbols, not namespace symbols. Use the
  // hook to record information for the resolution and
  // completion plugin.
  if (const auto sym = lhs_ns_sym->LinkedScope->GetVarSymbol(Name.get())) {
    visibility_utils::CheckModuleMemberVisibility(*sym, *Name, *lhs_ns_sym->LinkedScope, *sm, *meta);
    lsp::resolution_index::RecordVariable(*Name, *sm, *meta, sym);
  }
  else {
    lsp::resolution_index::RecordNamespaceMember(*Name, *sm, *meta, *lhs_ns_sym->LinkedScope);
  }
}

auto PostfixExpressionOperatorStaticMemberAccessAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Handle accessing a symbol on a type.
  if (_LhsTypeSym != nullptr) {
    const auto sym = StaticMemberOf(*_LhsTypeSym->LinkedScope, *Name);
    auto tm = ScopeManager(
      sm->GlobalScope, _LhsTypeSym->LinkedScope);
    sym->CompTimeValue->Stage9_CompTimeResolve(&tm, meta);
    meta->CmpResult = AstClone(meta->CmpResult);
    return;
  }

  // Handle accessing a variable on a namespace.
  // Todo: Do we need to call stage_9 on the value?
  const auto lhs_ns_sym = LhsNsScope(sm, meta)->NsSym;
  const auto sym = lhs_ns_sym->LinkedScope->GetVarSymbol(Name.get(), true);
  meta->CmpResult = AstClone(sym->CompTimeValue->To<ExpressionAst>());
}

auto PostfixExpressionOperatorStaticMemberAccessAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  const auto uid = "." + Uid();

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

    // A method named as a value is its "$" mock's constant, the
    // "{fn, env}" pair; without one there is nothing to load.
    if (var_sym->Kind == VariableKind::Function and var_sym->LlvmInfo->Alloca == nullptr) {
      return nullptr;
    }

    // A constant typed by a "sup" block's generic ("cmp n: T")
    // has no global (only the template is generated), so its
    // folded value is emitted in place.
    if (var_sym->LlvmInfo->Alloca == nullptr) {
      Stage9_CompTimeResolve(sm, meta);
      const auto folded = std::move(meta->CmpResult);
      SPP_ASSERT(folded != nullptr);

      // The value is the template's, written in terms of its own
      // parameters ("Unit[T]()", "Self()"), which name nothing
      // here; the instance and its arguments are what they stand
      // for.
      auto const &gn_args = _LhsTypeSym->Name->GnArgGroup;
      if (gn_args != nullptr and not gn_args->Args.IsEmpty()) {
        auto args = gn_args->GetAllArgs();
        const auto self_arg = GenericArgumentAst::NewType(
          generate::common_types::SelfType(PosStart()), _LhsTypeSym->FqName());
        args.EmplaceBack(self_arg.get());
        return folded->SubstituteGenericsExpr(args)->Stage11_CodeGen(sm, meta, ctx);
      }
      return folded->Stage11_CodeGen(sm, meta, ctx);
    }
    const auto global_var = codegen::GetOrAddGlobalIntoCurrentModule(
      *llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca),
      *codegen::GetEmissionModule(*ctx));
    return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.static_type_member" + uid);
  }

  // Namespace case: LHS is a namespace identifier — access a cmp constant in the namespace's scope.
  const auto lhs_ns_scope = LhsNsScope(sm, meta);
  const auto var_sym = lhs_ns_scope->GetVarSymbol(Name.get(), true);
  if (var_sym->Kind == VariableKind::Function) { return nullptr; }
  SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);
  const auto global_var = codegen::GetOrAddGlobalIntoCurrentModule(
    *llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca),
    *codegen::GetEmissionModule(*ctx));
  return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.static_ns_member" + uid);
}

auto PostfixExpressionOperatorStaticMemberAccessAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  //
  IMPORT_UTILS;
  // Todo: use the stored symbol? that if it's null? is that possible ie if used before analysis? shouldn't be.

  // Get the left-hand-side type's member's type.
  if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
    // todo: const auto sym = _LhsTypeSym->LinkedScope->GetVarSymbol(Name.get(), true);
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_as_type);
    const auto sym = StaticMemberOf(*lhs_type_sym->LinkedScope, *Name);

    // A method's "$" mock is declared in its "sup" block, so it is
    // named as a nested type of the owner: "main::A::$Method". There
    // is one mock per "sup" block the overloads are written in, each
    // given all of them ("ScopeManager::CoalesceMethodMock").
    if (sym != nullptr and sym->Kind == VariableKind::Function and sym->Type->IsTypeIdentifier()) {
      return MakeShared<TypePostfixExpressionAst>(
        AstCloneShared(lhs_type_sym->FqName()), MakeShared<TypePostfixExpressionOperatorNestedTypeAst>(
          nullptr, dynamic_shared_cast<TypeIdentifierAst>(AstCloneShared(sym->Type))));
    }
    if (sym != nullptr) { return sym->Type; }

    // This is where we need to handle the FwdRef/FwdMut logic.
    const auto [fwd_ref_sym, _] = marker_sups::GetFwdTypes(*lhs_type_sym, *sm->CurrentScope);
    const auto inner_type_sym = fwd_ref_sym->BoundTypeArg("T");
    const auto fwd_sym = inner_type_sym->LinkedScope->GetVarSymbol(Name.get(), true);
    return fwd_sym->Type;
  }

  // Get the left-hand-side namespace's member's type.
  const auto lhs_ns_scope = LhsNsScope(sm, meta);
  const auto type = lhs_ns_scope->GetVarSymbol(Name.get(), true)->Type;
  return lhs_ns_scope->GetTypeSymbol(type.get())->FqName();
}

auto PostfixExpressionOperatorStaticMemberAccessAst::LhsNsScope(
  ScopeManager const *sm, CompilerMetaData const *meta) -> Scope const* {
  // Resolve the namespace once, from where the access was written. A
  // parameter default is copied into each call site, and its lhs may
  // be relative to the callee's module ("cffi::x" in std), which the
  // caller's scope cannot reach.
  if (_LhsNsScope == nullptr) {
    _LhsNsScope = sm->CurrentScope->ConvertPostfixToNestedScope(meta->PostfixExpressionLhs);
  }
  return _LhsNsScope;
}

auto PostfixExpressionOperatorStaticMemberAccessAst::ExprParts() const -> Vec<IdentifierAst*> {
  // Static member access does not have any expression parts.
  return {Name.get()};
}

auto PostfixExpressionOperatorStaticMemberAccessAst::IsAllowedInDefault() const -> bool {
  // Reads what it is applied to, and holds nothing of its own.
  return true;
}

SPP_MOD_END
