module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_resolution;
import spp.analyse.utils.visibility_utils;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_ast;
import spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_fn;
import spp.lex.tokens;
import spp.lsp.resolution_index;
import spp.utils.interner;
import spp.utils.ptr;
import spp.utils.strings;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
PostfixExpressionOperatorStaticMemberAccessAst::PostfixExpressionOperatorStaticMemberAccessAst(
  decltype(TokDblColon) &&tok_dbl_colon,
  decltype(Name) &&name) :
  TokDblColon(std::move(tok_dbl_colon)),
  Name(std::move(name)),
  _LhsTypeSymbol(nullptr),
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
  p->_LhsTypeSymbol = _LhsTypeSymbol;
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
    const auto lhs_type_sym = sm->CurrentScope->FindTypeSymbol(lhs_as_type);
    _LhsTypeSymbol = lhs_type_sym;

    // Check the target field exists on the type.
    if (not lhs_type_sym->LinkedScope->HasVarSymbol(Name.get(), true)) {
      const auto lhs_ref = TypeRef::Of(*lhs_as_type, *sm->CurrentScope);
      auto *const lhs_fwd_ref_type_sym =
        marker_sups::FwdTargetOf(marker_sups::FindFwdSups(lhs_ref, *sm->CurrentScope).first).Symbol;
      const auto found = lhs_fwd_ref_type_sym != nullptr
        and lhs_fwd_ref_type_sym->LinkedScope->HasVarSymbol(Name.get(), true);
      if (not found) {
        // Todo: Need to filter these candidates to function groups who contain a static overload.
        // Todo: Add fwd-ref type member candidates

        auto candidates = lhs_type_sym->LinkedScope->GetAllVarSymbols(true, true)
          | genex::views::filter([](auto const &sym) { return sym->Kind == VariableKind::FnMock; })
          | genex::to<Vec>();
        member_lookup::RaiseMissingIdentifierAndClosestOptions(*Name, std::move(candidates), {}, *sm);
      }
      _LhsTypeSymbol = lhs_fwd_ref_type_sym;
    }

    // Check there is only 1 target field on the type at the highest level.
    if (const auto named = _LhsTypeSymbol->LinkedScope->FindVarSymbol(Name.get(), true);
      named->Kind == VariableKind::FnMock) {
      // Use the hook to record information for the resolution and
      // completion plugin.
      lsp::resolution_index::RecordVariable(*Name, *sm, *meta, named);
      return;
    }

    // A class attribute belongs to a value of the type rather
    // than to the type, so it is reached with "." instead.
    const auto member = member_lookup::MemberOf(*_LhsTypeSymbol->LinkedScope, *Name, MemberAccessForm::Static);
    RaiseIf<SppMemberAccessRuntimeOperatorExpectedError>(
      member != nullptr and not member_lookup::MemberReachableBy(*member, MemberAccessForm::Static), {sm->CurrentScope},
      ERR_ARGS(*Name, *TokDblColon, "attribute"));

    // Which declaration the access resolves to: the nearest
    // scope that can reach the name, supers included. Only
    // the members "::" reaches are in the running, so a class
    // attribute of the same name neither answers this nor makes
    // it ambiguous.
    const auto closest = member_lookup::ClosestMembers(*_LhsTypeSymbol->LinkedScope, *Name, MemberAccessForm::Static);

    // Enforce visibility on the accessed member. Visibility is
    // read off the non-generic scope, because that is where the
    // member was written and so where its annotation lives; an
    // instantiation's copy of a symbol is not the declaration.
    if (not closest.IsEmpty()) {
      const auto scope = closest[0].Where->NonGnScope;
      const auto declared_sym = scope->FindVarSymbol(Name.get());
      visibility_utils::CheckTypeMemberVisibility(
        declared_sym != nullptr ? *declared_sym : *closest[0].Symbol, *Name,
        declared_sym != nullptr ? *scope : *closest[0].Where, *sm, *meta);

      // Use the hook to record information for the resolution and
      // completion plugin.
      lsp::resolution_index::RecordVariable(
        *Name, *sm, *meta, declared_sym != nullptr ? declared_sym : closest[0].Symbol);
    }

    member_lookup::RaiseIfAmbiguous(closest, *Name, *sm);
    return;
  }

  // Otherwise, we are handling a namespace left-hand-side.
  const auto lhs_as_ident = meta->PostfixExpressionLhs->To<IdentifierAst>();
  const auto lhs_var_sym = sm->CurrentScope->FindVarSymbol(lhs_as_ident);

  // Check the lhs is a namespace and not a variable.
  RaiseIf<SppMemberAccessRuntimeOperatorExpectedError>(
    lhs_var_sym != nullptr, {sm->CurrentScope},
    ERR_ARGS(*meta->PostfixExpressionLhs, *TokDblColon, "variable"));

  // Check the constant exists inside the namespace.
  // Todo: inconsistent "true" for exclusive here vs ns
  const auto lhs_ns_sym = LhsNsScope(sm, meta)->LinkedNamespaceSymbol;
  if (not lhs_ns_sym->LinkedScope->HasVarSymbol(Name.get(), true) and not lhs_ns_sym->LinkedScope->HasNsSymbol(
    Name.get(), true)) {
    member_lookup::RaiseMissingIdentifierAndClosestOptions(
      *Name, lhs_ns_sym->LinkedScope->GetAllVarSymbols(false, true), lhs_ns_sym->LinkedScope->GetAllNsSymbols(), *sm);
  }

  // Enforce visibility on the accessed namespace symbol.
  // Only for var symbols, not namespace symbols. Use the
  // hook to record information for the resolution and
  // completion plugin.
  if (const auto sym = lhs_ns_sym->LinkedScope->FindVarSymbol(Name.get())) {
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
  if (_LhsTypeSymbol != nullptr) {
    // Read by its identity where that is a value ("scopes::CompMemberIdOf"): a constant whose value names its
    // block's parameters ("k + 1_uz") is read in this instantiation's block, where they are bound.
    if (const auto owner = WrittenTypeIdOf(*_LhsTypeSymbol); analyse::scopes::IsClosedTypeId(owner)) {
      if (const auto read = analyse::scopes::CompMemberIdOf(owner, Name->ToView()); read != 0) {
        if (const auto value = sm->CurrentScope->CompAstOf(read);
          value != nullptr and value->To<LiteralAst>() != nullptr) {
          meta->CompTimeResult = AstClone(value.get());
          return;
        }
      }
    }
    const auto sym = analyse::utils::member_lookup::MemberOf(
      *_LhsTypeSymbol->LinkedScope, *Name, analyse::utils::member_lookup::MemberAccessForm::Static);
    auto tm = ScopeManager(
      sm->GlobalScope, _LhsTypeSymbol->LinkedScope);
    sym->CompTimeValue->Stage9_CompTimeResolve(&tm, meta);
    meta->CompTimeResult = AstClone(meta->CompTimeResult);
    return;
  }

  // Handle accessing a variable on a namespace.
  // Todo: Do we need to call stage_9 on the value?
  const auto lhs_ns_sym = LhsNsScope(sm, meta)->LinkedNamespaceSymbol;
  const auto sym = lhs_ns_sym->LinkedScope->FindVarSymbol(Name.get(), true);
  meta->CompTimeResult = AstClone(sym->CompTimeValue->To<ExpressionAst>());
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
    if (const auto folded = std::move(meta->CompTimeResult); folded != nullptr) {
      return folded->Stage11_CodeGen(sm, meta, ctx);
    }
  }

  // Type case: LHS is a TypeAst — access a cmp constant on the type's scope.
  if (_LhsTypeSymbol != nullptr) {
    const auto var_sym = analyse::utils::member_lookup::MemberOf(
      *_LhsTypeSymbol->LinkedScope, *Name, analyse::utils::member_lookup::MemberAccessForm::Static);

    // A method named as a value is its "$" mock's constant, the
    // "{fn, env}" pair; without one there is nothing to load.
    if (var_sym->Kind == VariableKind::FnMock and var_sym->LlvmInfo->Alloca == nullptr) {
      return nullptr;
    }

    // A constant typed by a "sup" block's generic ("cmp n: T")
    // has no global (only the template is generated), so its
    // folded value is emitted in place.
    if (var_sym->LlvmInfo->Alloca == nullptr) {
      Stage9_CompTimeResolve(sm, meta);
      const auto folded = std::move(meta->CompTimeResult);
      SPP_ASSERT(folded != nullptr);

      // The value is the template's, written in terms of its block's parameters ("Unit[T]()", "Self()"), which name
      // nothing here. It is read in the block that declares it for this instance, where they are bound, with "Self"
      // as the type named.
      if (_LhsTypeSymbol->InstanceOf != nullptr) {
        const auto declaring = member_lookup::ScopesDeclaringVar(*_LhsTypeSymbol->LinkedScope, *Name);
        const auto in = genex::find_if(declaring, [var_sym](auto const &d) { return d.Symbol == var_sym; });
        auto bindings = analyse::scopes::GenericSubst();
        type_resolution::BindSelf(bindings, *_LhsTypeSymbol->FqName(), *sm->CurrentScope);
        return folded->ReadExpr(analyse::scopes::ExprSubst::Across(
          in != declaring.end() ? *in->Where : *_LhsTypeSymbol->LinkedScope, std::move(bindings), *sm->CurrentScope))
          ->Stage11_CodeGen(sm, meta, ctx);
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
  const auto var_sym = lhs_ns_scope->FindVarSymbol(Name.get(), true);
  if (var_sym->Kind == VariableKind::FnMock) { return nullptr; }
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
    const auto lhs_type_sym = sm->CurrentScope->FindTypeSymbol(lhs_as_type);
    const auto sym = analyse::utils::member_lookup::MemberOf(
      *lhs_type_sym->LinkedScope, *Name, analyse::utils::member_lookup::MemberAccessForm::Static);

    // A method's "$" mock is declared in its "sup" block, so it is
    // named as a nested type of the owner: "main::A::$Method". There
    // is one mock per "sup" block the overloads are written in, each
    // given all of them ("ScopeManager::CoalesceMethodMock").
    if (sym != nullptr and sym->Kind == VariableKind::FnMock and sym->Type->IsTypeIdentifier()) {
      return MakeShared<TypePostfixExpressionAst>(
        AstCloneShared(lhs_type_sym->FqName()), MakeShared<TypePostfixExpressionOperatorNestedTypeAst>(
          nullptr, dynamic_shared_cast<TypeIdentifierAst>(AstCloneShared(sym->Type))));
    }
    if (sym != nullptr) { return sym->Type; }

    // This is where we need to handle the FwdRef/FwdMut logic.
    const auto lhs_ref = TypeRef::Of(*lhs_as_type, *sm->CurrentScope);
    const auto inner_type_sym =
      marker_sups::FwdTargetOf(marker_sups::FindFwdSups(lhs_ref, *sm->CurrentScope).first).Symbol;
    const auto fwd_sym = inner_type_sym->LinkedScope->FindVarSymbol(Name.get(), true);
    return fwd_sym->Type;
  }

  // Get the left-hand-side namespace's member's type.
  const auto lhs_ns_scope = LhsNsScope(sm, meta);
  const auto type = lhs_ns_scope->FindVarSymbol(Name.get(), true)->Type;
  return lhs_ns_scope->FindTypeSymbol(type.get())->FqName();
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
