module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
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
import spp.utils.ice;
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

  // The member is looked up here, in the scope the access is analysed in, and reused from now on.
  _MemberScope = nullptr;

  // Handle types on the left-hand-side of a static member access.
  if (Lhs->To<TypeAst>() != nullptr) {
    const auto member = _Resolve(sm);
    // The member exists on the type, or on the type it forwards to. The candidates are the methods of either: a
    // static access names a method or a constant (Todo: filter to the methods with a static overload).
    if (member == nullptr) {
      auto candidates = _LhsTypeSymbol->LinkedScope->GetAllVarSymbols(true, true);
      const auto lhs_ref = TypeRef::Of(*Lhs->As<TypeAst>(), *sm->CurrentScope);
      const auto fwd_sups = marker_sups::FindFwdSups(lhs_ref, *sm->CurrentScope);
      if (auto const *const fwd = marker_sups::FwdTargetOf(fwd_sups.first).Symbol;
        fwd != nullptr and fwd->LinkedScope != nullptr) {
        candidates.append_range(fwd->LinkedScope->GetAllVarSymbols(true, true));
      }
      member_lookup::RaiseMissingIdentifierAndClosestOptions(
        *Name, candidates | genex::views::filter([](auto const *sym) { return sym->Kind == VariableKind::FnMock; })
          | genex::to<Vec>(), {});
    }

    // A method named as a value: its "$" mock. Use the hook to record information for the resolution and completion
    // plugin.
    if (member->Kind == VariableKind::FnMock) {
      lsp::resolution_index::RecordVariable(*Name, *sm, *meta, member);
      return;
    }

    // A class attribute belongs to a value of the type rather
    // than to the type, so it is reached with "." instead.
    RaiseIf<SppMemberAccessRuntimeOperatorExpectedError>(
      not member_lookup::MemberReachableBy(*member, MemberAccessForm::Static),
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
    if (not closest.empty()) {
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

    member_lookup::RaiseIfAmbiguous(closest, *Name);
    return;
  }

  // Otherwise, we are handling a namespace left-hand-side, which must not name a variable.
  RaiseIf<SppMemberAccessRuntimeOperatorExpectedError>(
    sm->CurrentScope->FindVarSymbol(Lhs->To<IdentifierAst>()) != nullptr,
    ERR_ARGS(*Lhs, *TokDblColon, "variable"));
  const auto member = _Resolve(sm);

  // The name is a constant or a namespace of the namespace's own: "ns::x" reaches nothing the namespace's parents
  // declare, so neither does a suggestion.
  if (member == nullptr and not _MemberScope->HasNsSymbol(Name.get(), true)) {
    member_lookup::RaiseMissingIdentifierAndClosestOptions(
      *Name, _MemberScope->GetAllVarSymbols(true), _MemberScope->GetAllNsSymbols(true));
  }

  // Enforce visibility on the accessed namespace symbol.
  // Only for var symbols, not namespace symbols. Use the
  // hook to record information for the resolution and
  // completion plugin.
  if (member != nullptr) {
    visibility_utils::CheckModuleMemberVisibility(*member, *Name, *_MemberScope, *sm, *meta);
    lsp::resolution_index::RecordVariable(*Name, *sm, *meta, member);
  }
  else {
    lsp::resolution_index::RecordNamespaceMember(*Name, *sm, *meta, *_MemberScope);
  }
}

auto PostfixExpressionOperatorStaticMemberAccessAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Handle accessing a symbol on a type.
  if (_LhsTypeSymbol != nullptr) {
    if (const auto owner = NameTypeIdOf(*_LhsTypeSymbol); IsClosedTypeId(owner)) {
      if (const auto read = comp_generics::FindCompMemberId(owner, Name->ToView()); read != nullptr) {
        if (const auto value = sm->CurrentScope->CompAstOf(read);
          value != nullptr and value->To<LiteralAst>() != nullptr) {
          meta->CompTimeResult = AstClone(value.get());
          return;
        }
      }
    }
    const auto sym = _Resolve(sm);

    // Prevent loops of "cmp a = Self::b" and "cmp b = Self::a",
    // as this is impossiblt to provide a value.
    using comp_generics::CompTimeResolutionGuard;
    RaiseIf<SppCompTimeConstantError>(
      CompTimeResolutionGuard::IsResolving(sym),
      ERR_ARGS(*this));
    const auto _guard = CompTimeResolutionGuard(sym);
    auto tm = sm->NewManagerAt(_LhsTypeSymbol->LinkedScope);
    sym->CompTimeValue->Stage9_CompTimeResolve(&tm, meta);
    meta->CompTimeResult = AstClone(meta->CompTimeResult);
    return;
  }

  // Handle accessing a variable on a namespace.
  // Todo: Do we need to call stage_9 on the value?
  meta->CompTimeResult = AstClone(_Resolve(sm)->CompTimeValue->To<ExpressionAst>());
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
    const auto var_sym = _Resolve(sm);

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
        auto bindings = GenericSubst();
        BindSelf(bindings, *_LhsTypeSymbol->FqName(), *sm->CurrentScope);
        return folded->ReadExpr(ExprSubst::Across(
          in != declaring.end() ? *in->Where : *_LhsTypeSymbol->LinkedScope, std::move(bindings), *sm->CurrentScope))
          ->Stage11_CodeGen(sm, meta, ctx);
      }
      return folded->Stage11_CodeGen(sm, meta, ctx);
    }
    return codegen::LoadGlobalConstant(*var_sym->LlvmInfo, "load.static_type_member" + uid, ctx);
  }

  // Namespace case: LHS is a namespace identifier — access a cmp constant in the namespace's scope.
  const auto var_sym = _Resolve(sm);
  if (var_sym->Kind == VariableKind::FnMock) { return nullptr; }
  SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);
  return codegen::LoadGlobalConstant(*var_sym->LlvmInfo, "load.static_ns_member" + uid, ctx);
}

auto PostfixExpressionOperatorStaticMemberAccessAst::InferType(
  ScopeManager *sm, CompilerMetaData *) -> Shared<TypeAst> {
  //
  IMPORT_UTILS;
  const auto sym = _Resolve(sm);

  // A method's "$" mock is declared in its "sup" block, so it is
  // named as a nested type of the owner: "main::A::$Method". There
  // is one mock per "sup" block the overloads are written in, each
  // given all of them ("ScopeManager::CoalesceMethodMock").
  if (_LhsTypeSymbol != nullptr) {
    if (sym->Kind == VariableKind::FnMock and sym->Type->IsTypeIdentifier()) {
      return MakeShared<TypePostfixExpressionAst>(
        AstCloneShared(_LhsTypeSymbol->FqName()), MakeShared<TypePostfixExpressionOperatorNestedTypeAst>(
          nullptr, shared_cast<TypeIdentifierAst>(AstCloneShared(sym->Type))));
    }
    return sym->Type;
  }

  // A namespace's member's type, as its namespace names it.
  return _MemberScope->FindTypeSymbol(sym->Type.get())->FqName();
}

auto PostfixExpressionOperatorStaticMemberAccessAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // A member is read where its owner (or the type it forwards to) declares it, which binds the parameters its type
  // names; a method's "$" mock is named as a nested type of the owner ("InferType"). A namespace member is read in
  // its namespace.
  IMPORT_UTILS;
  const auto sym = _Resolve(sm);
  if (_LhsTypeSymbol != nullptr and sym->Kind == VariableKind::FnMock and sym->Type->IsTypeIdentifier()) {
    return TypeRef::Of(*InferType(sm, meta), *sm->CurrentScope);
  }
  return sym->TypeRefIn(*_MemberScope);
}

auto PostfixExpressionOperatorStaticMemberAccessAst::LhsNsScope(
  ScopeManager const *sm) -> Scope const* {
  // Resolve the namespace once, from where the access was written. A
  // parameter default is copied into each call site, and its lhs may
  // be relative to the callee's module ("cffi::x" in std), which the
  // caller's scope cannot reach.
  if (_LhsNsScope == nullptr) {
    _LhsNsScope = sm->CurrentScope->FindNsScope(Lhs);
  }
  return _LhsNsScope;
}

auto PostfixExpressionOperatorStaticMemberAccessAst::_Resolve(
  ScopeManager const *sm) -> VariableSymbol* {
  //
  IMPORT_UTILS;
  using member_lookup::MemberAccessForm;
  if (_MemberScope != nullptr) { return _Member; }

  // A namespace's own constant.
  const auto lhs_as_type = Lhs->To<TypeAst>();
  if (lhs_as_type == nullptr) {
    _MemberScope = LhsNsScope(sm);
    _Member = _MemberScope->FindVarSymbol(Name.get(), true);
    return _Member;
  }

  // A type's member as "::" reaches it, else the forwarded-to type's.
  _LhsTypeSymbol = sm->CurrentScope->FindTypeSymbol(lhs_as_type);
  _Member = member_lookup::MemberOf(*_LhsTypeSymbol->LinkedScope, *Name, MemberAccessForm::Static);
  if (_Member == nullptr) {
    const auto lhs_ref = TypeRef::Of(*lhs_as_type, *sm->CurrentScope);
    auto *const fwd = marker_sups::FwdTargetOf(marker_sups::FindFwdSups(lhs_ref, *sm->CurrentScope).first).Symbol;
    if (fwd != nullptr and fwd->LinkedScope != nullptr) {
      if (const auto via_fwd = member_lookup::MemberOf(*fwd->LinkedScope, *Name, MemberAccessForm::Static)) {
        _LhsTypeSymbol = fwd;
        _Member = via_fwd;
      }
    }
  }
  _MemberScope = _LhsTypeSymbol->LinkedScope;
  return _Member;
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
