module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.visibility_utils;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_func;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.algorithms;
import spp.utils.strings;
import spp.utils.uid;
import genex;

namespace {
  auto RuntimeMemberOf(
    spp::analyse::scopes::Scope &type_scope,
    spp::asts::IdentifierAst const &name)
    -> spp::analyse::scopes::VariableSymbol* {
    //
    using spp::analyse::utils::expr_utils::LookupMemberForAccess;
    using spp::analyse::utils::expr_utils::MemberAccessForm;
    using spp::analyse::utils::expr_utils::MemberReachableBy;

    // If the scope symbol exists (nullptr check for type
    // forwarding), and the member can be runtime-accessed,
    // then return the found symbol.
    const auto found = type_scope.GetVarSymbol(&name);
    if (found == nullptr or MemberReachableBy(
      *found, MemberAccessForm::Runtime)) { return found; }

    // If the cheap check gave a static symbol, then search
    // more deeply through the super scopes to find the
    // member in a runtime context.
    const auto member = LookupMemberForAccess(
      type_scope, name, MemberAccessForm::Runtime);
    return member != nullptr ? member : found;
  }
}

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PostfixExpressionOperatorRuntimeMemberAccessAst(
  decltype(TokDot) &&tok_dot,
  decltype(Name) name) :
  TokDot(std::move(tok_dot)),
  Name(std::move(name)),
  _MappedFwd(nullptr) {
  Source.OriginalExpr = nullptr;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokDot, lex::SppTokenType::TK_DOT, ".");
}

spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::~PostfixExpressionOperatorRuntimeMemberAccessAst() = default
;

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PosStart() const
  -> std::size_t {
  // Use the "." token.
  return TokDot->PosStart();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PosEnd() const
  -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast, sharing the mapped
  // forwarding access so a clone taken after analysis
  // keeps it.
  auto ast = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    AstClone(TokDot),
    AstClone(Name));
  ast->_MappedFwd = _MappedFwd;
  ast->Source.OriginalExpr = Source.OriginalExpr;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokDot);
  SPP_STRING_APPEND(Name);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppExpressionNotTryError;
  using analyse::errors::SppMemberAccessNonIndexableError;
  using analyse::errors::SppMemberAccessOutOfBoundsError;
  using analyse::errors::SppMemberAccessStaticOperatorExpectedError;
  using analyse::utils::expr_utils::ClosestScopes;
  using analyse::utils::expr_utils::MemberAccessForm;
  using analyse::utils::expr_utils::MembersReachableBy;
  using analyse::utils::expr_utils::RaiseIfAmbiguous;
  using analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions;
  using analyse::utils::expr_utils::ScopesDeclaringVar;
  using analyse::utils::type_predicates::IsTypeCompTimeIndexable;
  using analyse::utils::type_predicates::IsIndexWithinBound;
  using analyse::utils::type_utils::BuildFwdCall;
  using analyse::utils::visibility_utils::CheckTypeMemberVisibility;
  using analyse::utils::visibility_utils::IsTypeMemberVisible;

  // Already rewritten against a forwarded-to value by an
  // earlier pass, which analysed the rewrite as it built
  // it.
  if (_MappedFwd != nullptr) { return; }

  // Prevent types on the left-hand-side of a runtime
  // member access.
  RaiseIf<SppMemberAccessStaticOperatorExpectedError>(
    meta->PostfixExpressionLhs->To<TypeAst>() != nullptr,
    {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *TokDot, "type"));

  // Numeric index access (for tuples).
  if (std::isdigit(Name->Val[0])) {
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);

    // Check the lhs is a tuple/array (the only indexable
    // types).
    RaiseIf<SppMemberAccessNonIndexableError>(
      not IsTypeCompTimeIndexable(*lhs_type, *sm->CurrentScope),
      {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, *TokDot));

    // Check the index is within the bounds of the tuple
    // or array.
    auto [in_bounds, n] = IsIndexWithinBound(std::stoul(Name->Val), *lhs_type, *sm->CurrentScope);
    RaiseIf<SppMemberAccessOutOfBoundsError>(
      not in_bounds,
      {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, n, *TokDot));
  }

  // Accessing a regular attribute/method on an instance.
  else {
    const auto lhs_as_ident_raw = meta->PostfixExpressionLhs->To<IdentifierAst>();
    const auto lhs_as_ident = lhs_as_ident_raw
      ? MakeShared<IdentifierAst>(lhs_as_ident_raw->PosStart(), lhs_as_ident_raw->Val)
      : nullptr;
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);

    const auto lhs_ns_sym = sm->CurrentScope->GetNsSymbol(lhs_as_ident.get());
    const auto lhs_var_sym = sm->CurrentScope->GetVarSymbol(lhs_as_ident.get());
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type.get());

    // Check the lhs is a variable and not a namespace.
    RaiseIf<SppMemberAccessStaticOperatorExpectedError>(
      // Todo: this error message uses "Type" -> accept param for ctx (ns)
      lhs_var_sym == nullptr and lhs_ns_sym != nullptr, {sm->CurrentScope},
      ERR_ARGS(*meta->PostfixExpressionLhs, *TokDot, "namespace"));

    // Check whether the target field exists on the type,
    // or on the forwarded type.
    if (not lhs_type_sym->LinkedScope->HasVarSymbol(Name.get(), true)) {
      // If we are accessing via forwarding, then build the
      // forward call, and store it for later analysis.
      auto fwd_call = BuildFwdCall(*meta->PostfixExpressionLhs, *lhs_type, sm, meta);
      if (fwd_call != nullptr) {
        _MappedFwd = MakeShared<PostfixExpressionAst>(
          std::move(fwd_call),
          MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, Name));
        _MappedFwd->Stage7_AnalyseSemantics(sm, meta);
        return;
      }

      // An access the "?" lowering, generated to reach a "Try"
      // member, so failing to find it means the operand is not
      // a "Try" type - provide a more refined error.
      const auto try_op = AstAs<PostfixExpressionOperatorEarlyReturnAst>(Source.OriginalExpr);
      RaiseIf<SppExpressionNotTryError>(
        try_op != nullptr, {sm->CurrentScope},
        ERR_ARGS(*try_op, *lhs_type));

      // Type field was not found on this type, or the
      // forwarding type (includes nested forwarding checks).
      RaiseMissingIdentifierAndClosestOptions(
        *Name, lhs_type_sym->LinkedScope->AllVarSymbols(true, true), {}, *sm);
    }

    auto all_scopes_and_syms = ScopesDeclaringVar(
      *lhs_type_sym->LinkedScope, *Name, false);

    // Enforce visibility on functional (method) members.
    // Their mock ("$"-typed) symbols are excluded from the
    // attribute handling below, so without this the
    // visibility check never runs for method accesses.
    auto fn_scopes_and_syms = all_scopes_and_syms
      | genex::views::filter([](auto const &x) { return x.Symbol->Type->IsCompilerGeneratedType(); })
      | genex::to<Vec>();

    if (not fn_scopes_and_syms.IsEmpty()) {
      const auto cls_scope = lhs_type_sym->LinkedScope->NonGenericScope;
      const auto any_visible = genex::any_of(fn_scopes_and_syms, [&](auto const &x) {
        return IsTypeMemberVisible(*x.Symbol, *cls_scope, *sm, *meta);
      });
      if (not any_visible) {
        CheckTypeMemberVisibility(*fn_scopes_and_syms.Back().Symbol, *Name, *cls_scope, *sm, *meta);
      }
    }

    const auto members = all_scopes_and_syms
      | genex::views::filter([](auto const &x) { return not x.Symbol->Type->IsCompilerGeneratedType(); })
      | genex::to<Vec>();

    const auto runtime_members = MembersReachableBy(
      members, MemberAccessForm::Runtime);

    // A "cmp" constant belongs to the type rather than to any
    // value of it, so it is reached with "::" instead. This
    // is also what picks an attribute out where the type declares
    // a constant of the same name: the two are different members,
    // and only how the access is written says which one was meant.
    RaiseIf<SppMemberAccessStaticOperatorExpectedError>(
      runtime_members.IsEmpty() and not members.IsEmpty(), {sm->CurrentScope},
      ERR_ARGS(*Name, *TokDot, "constant"));

    // If we only have functional types, just return.
    if (runtime_members.Len() < 1) { return; }
    const auto closest = ClosestScopes(runtime_members);

    // Enforce visibility on the accessed member.
    if (not closest.IsEmpty()) {
      const auto scope = closest[0].Where->NonGenericScope;
      CheckTypeMemberVisibility(*scope->GetVarSymbol(Name.get(), true), *Name, *scope, *sm, *meta);
    }

    RaiseIfAmbiguous(closest, *Name, *sm);
  }
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::cmp_utils::GetCompTimeAttrValue;

  // A member reached by forwarding is resolved against the
  // forwarded-to value, which the rewritten access names.
  if (_MappedFwd != nullptr) {
    _MappedFwd->Stage9_CompTimeResolve(sm, meta);
    return;
  }

  // Resolve the left-hand-side expression.
  meta->PostfixExpressionLhs->Stage9_CompTimeResolve(sm, meta);

  // Handle numeric index access (for tuples).
  if (std::isdigit(Name->Val[0]) and meta->CmpResult->To<TupleLiteralAst>()) {
    const auto cmp_tup = meta->CmpResult->To<TupleLiteralAst>();
    const auto index = std::stoul(Name->Val);
    auto cmp_field = AstClone(cmp_tup->Elems[index]);
    meta->CmpResult = std::move(cmp_field);
    return;
  }

  // Handle numeric index access (for arrays).
  if (std::isdigit(Name->Val[0]) and meta->CmpResult->To<ArrayLiteralExplicitElementsAst>()) {
    const auto cmp_tup = meta->CmpResult->To<ArrayLiteralExplicitElementsAst>();
    const auto index = std::stoul(Name->Val);
    auto cmp_field = AstClone(cmp_tup->Elems[index]);
    meta->CmpResult = std::move(cmp_field);
    return;
  }

  // Handle normal attribute access (for objects).
  const auto cmp_obj = meta->CmpResult->To<ObjectInitializerAst>();
  meta->CmpResult = GetCompTimeAttrValue(cmp_obj, Name.get());
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  //
  using analyse::utils::type_members::GetFieldIndexInType;
  using analyse::utils::type_predicates::IsTypeArr;

  // A member reached by forwarding lives on the forwarded-to
  // value, so the mapped ast generates it: the forwarding call
  // it is applied to produces the borrow that is then indexed
  // into.
  if (_MappedFwd != nullptr) { return _MappedFwd->Stage11_CodeGen(sm, meta, ctx); }

  // This expression names storage, so it can produce either
  // the address of the field or the value held in it. The
  // consume ast picks: assignment targets and borrows want
  // the address, every other context wants the value.
  const auto want_address = meta->LlvmWantAddress;

  // Get the type of the left-hand-side expression.
  const auto uid = "." + spp::utils::Uid(this);
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type.get());

  // Index through the object's own type, not a borrow's pointer
  // type.
  const auto is_borrow = lhs_type->GetConvention() != nullptr;
  const auto llvm_type = lhs_type_sym->LlvmInfo->LlvmType;
  SPP_ASSERT(llvm_type != nullptr);
  const auto lhs_is_member_access = IsRuntimeMemberAccess(meta->PostfixExpressionLhs);

  meta->Save();
  meta->LlvmWantAddress = lhs_is_member_access;

  // For attribute access on an object, the base pointer will
  // be the field immediately left of this specific access
  // operator. For "a.b.c", it is "a.b" etc.
  auto base_ptr = static_cast<llvm::Value*>(nullptr);
  if (lhs_is_member_access) {
    base_ptr = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
    if (is_borrow) {
      base_ptr = ctx->Builder.CreateLoad(
        llvm::PointerType::get(*ctx->Context, 0), base_ptr, "load.member_access.base_ptr" + uid);
    }
  }

  // If the lhs is symbolic, get the address of the outermost
  // part. The symbol's alloca is already the address of the
  // object (the base pointer). Load borrows to get value.
  else if (const auto sym = sm->CurrentScope->GetVarSymbolOutermost(*meta->PostfixExpressionLhs).first;
    sym != nullptr) {
    SPP_ASSERT(sym->LlvmInfo->Alloca != nullptr);

    // A "cmp" constant is a global, and a global belongs to
    // the one module that defines it, so indexing into one
    // from another module goes through that module's own
    // declaration of the symbol.
    auto object_ptr = sym->LlvmInfo->Alloca;
    if (const auto global_var = llvm::dyn_cast_or_null<llvm::GlobalVariable>(object_ptr); global_var != nullptr) {
      object_ptr = codegen::GetOrAddGlobalIntoCurrentModule(*global_var, *codegen::GetEmissionModule(*ctx));
    }

    base_ptr = is_borrow
      ? ctx->Builder.CreateLoad(
        llvm::PointerType::get(*ctx->Context, 0), object_ptr, "load.member_access.base_ptr" + uid)
      : object_ptr;
  }

  // A borrowed expression already evaluates to the address
  // of the object.
  else if (is_borrow) {
    base_ptr = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
  }

  // Materialize the lhs expression into a temporary, to have
  // an address to index through.
  else {
    const auto lhs_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
    const auto temp = codegen::LlvmEntryAlloca(llvm_type, "temp.member_access.lhs" + uid, ctx);
    ctx->Builder.CreateStore(lhs_val, temp);
    base_ptr = temp;
  }
  meta->Restore();

  // A field carrying no value is not laid out, so there is
  // nothing to index to and nothing to read: llvm has no value
  // of that type, no member for it in the struct, and "load
  // void" is not valid ir.
  const auto field_type = InferType(sm, meta);
  const auto field_llvm_type = sm->CurrentScope->GetTypeSymbol(
    field_type.get())->LlvmInfo->LlvmType;
  if (codegen::IsValuelessType(field_llvm_type)) { return nullptr; }

  // Resolve the address of the member. A numeric name indexes
  // a tuple or array positionally; any other name is an
  // attribute, whose physical position depends on how the
  // owning type was laid out.
  auto field_ptr = static_cast<llvm::Value*>(nullptr);
  if (std::isdigit(Name->Val[0])) {
    const auto index = static_cast<std::uint32_t>(std::stoul(Name->Val));

    // An array lowers to "[n x T]" rather than to a struct,
    // so it is indexed through the array itself: the leading
    // zero index steps over the pointer to the array, and the
    // second one selects the element.
    if (IsTypeArr(*lhs_type->WithoutConvention(), *sm->CurrentScope)) {
      const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
      field_ptr = ctx->Builder.CreateGEP(
        llvm_type, base_ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)},
        "member_access.arr.elem_ptr" + uid);
    }

    // A tuple lowers to a struct whose fields keep declaration
    // order, so element "n" is field "n".
    else {
      field_ptr = ctx->Builder.CreateStructGEP(llvm_type, base_ptr, index, "member_access.tup.elem_ptr" + uid);
    }
  }

  else {
    // The physical field order isn't the declaration order,
    // because the S++ layout re-orders the fields to minimize
    // padding, so the declaration index has to be resolved
    // through the type's field index map.
    const auto decl_index = GetFieldIndexInType(*lhs_type, *Name, *sm->CurrentScope);
    const auto field_index = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, decl_index);
    field_ptr = ctx->Builder.CreateStructGEP(llvm_type, base_ptr, field_index, "member_access.field_ptr" + uid);
  }
  if (want_address) { return field_ptr; }

  // Otherwise read the field out. Fields are never borrows
  // (the second class borrow rules forbid storing one), so
  // the field's own lowered type is always the type held in
  // the slot.
  return ctx->Builder.CreateLoad(field_llvm_type, field_ptr, "member_access.field" + uid);
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  //
  using analyse::utils::type_predicates::GetNthTypeOfIndexableType;

  // A member reached by forwarding belongs to the forwarded-to
  // type, so the rewritten access knows its type.
  if (_MappedFwd != nullptr) { return _MappedFwd->InferType(sm, meta); }

  // Get the type of the left-hand-side expression.
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);

  // Numeric index access (for tuples).
  if (std::isdigit(Name->Val[0])) {
    const auto elem_type = GetNthTypeOfIndexableType(
      std::stoul(Name->Val), *lhs_type, *sm->CurrentScope);
    return elem_type;
  }

  // Get the field symbol and return its type. Resolved by
  // access form, so that an attribute is what this reads on
  // a type that also declares a constant of that name.
  const auto lhs_sym = sm->CurrentScope->GetTypeSymbol(lhs_type.get());
  const auto var_sym = RuntimeMemberOf(*lhs_sym->LinkedScope, *Name);
  const auto field_type = var_sym->Type;
  return lhs_sym->LinkedScope->GetTypeSymbol(field_type.get())->FqName();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::GetFwdReceiver() const
  -> PostfixExpressionAst* {
  // The lhs of the rewritten access is the forwarding call
  // ("x.fwd_ref()") applied to the original lhs.
  return _MappedFwd != nullptr ? _MappedFwd->Lhs->To<PostfixExpressionAst>() : nullptr;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::ExprParts() const
  -> Vec<Ast*> {
  return {Name.get()};
}

SPP_MOD_END
