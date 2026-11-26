module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.analyse.utils.visibility_utils;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_drop;
import spp.codegen.llvm_fn;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.lsp.resolution_index;
import spp.utils.ice;
import spp.utils.strings;
import spp.utils.uid;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::analyse::scopes, VariableKind);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

namespace spp::asts {
  /// The left-hand side's symbol, when it names a template's open pack ("PackInfo::Open"): a function pack ("..xs:
  /// S32") or a comp pack ("cmp ..ns: Bool"). There "xs.0" reads an element, of the type the pack declares; in an
  /// instantiation it is the tuple, read like any other.
  static auto UnboundPack(
    spp::asts::ExpressionAst const &lhs,
    spp::analyse::scopes::ScopeManager const &sm)
    -> spp::analyse::scopes::VariableSymbol const*;
}

auto spp::asts::UnboundPack(
  spp::asts::ExpressionAst const &lhs,
  spp::analyse::scopes::ScopeManager const &sm)
  -> spp::analyse::scopes::VariableSymbol const* {
  auto const *const name = lhs.To<spp::asts::IdentifierAst>();
  auto const *const sym = name != nullptr ? sm.CurrentScope->FindVarSymbol(name) : nullptr;
  return sym != nullptr and sym->Pack.has_value() and sym->Pack->Open ? sym : nullptr;
}

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PostfixExpressionOperatorRuntimeMemberAccessAst(
  decltype(TokDot) &&tok_dot,
  decltype(Name) name) :
  TokDot(std::move(tok_dot)),
  Name(std::move(name)),
  _MappedFwd(nullptr) {
  Source.OriginalExpr = nullptr;
  Source.Lowered = false;

  // A generated access ("tmp.x" from a destructure) has no "."
  // written, so it is placed on the name it accesses.
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokDot, lex::SppTokenType::TK_DOT, ".", this->Name != nullptr ? this->Name->PosStart() : 0);
}

spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::~PostfixExpressionOperatorRuntimeMemberAccessAst() = default
;

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PosStart() const -> std::size_t {
  // Use the "." token.
  return TokDot->PosStart();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PosEnd() const -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, sharing the mapped
  // forwarding access so a clone taken after analysis
  // keeps it.
  auto ast = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    AstClone(TokDot),
    AstClone(Name));
  ast->_MappedFwd = _MappedFwd;
  ast->Source.OriginalExpr = Source.OriginalExpr;
  ast->Source.Lowered = Source.Lowered;
  ast->DropsReceiverRest = DropsReceiverRest;
  ast->ReadOnwards = ReadOnwards;
  ast->ReadByBorrow = ReadByBorrow;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokDot);
  SPP_STRING_APPEND(Name);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Already rewritten against a forwarded-to value by an
  // earlier pass, which analysed the rewrite as it built
  // it.
  if (_MappedFwd != nullptr) { return; }

  // A type is reached with "::", except a zero type, whose name
  // is its one value ("GlobalAlloc.allocate(..)", "None").
  {
    auto const *const lhs_type = Lhs->To<TypeAst>();
    auto const *const lhs_sym = lhs_type != nullptr ? sm->CurrentScope->FindTypeSymbol(lhs_type) : nullptr;
    RaiseIf<SppMemberAccessStaticOperatorExpectedError>(
      lhs_type != nullptr and not (lhs_sym != nullptr and lhs_sym->IsZeroType()),
      ERR_ARGS(*Lhs, *TokDot, "type"));
  }

  if (std::isdigit(Name->Val[0])) { _CheckIndexAccess(sm, meta); }
  else { _CheckNamedAccess(sm, meta); }
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::_CheckIndexAccess(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // An unbound pack's length is only known per instantiation,
  // where it is a tuple and is checked as one; here it only has
  // an element type. An open tuple ("Tup[Ts]", a variadic
  // parameter as a whole) has no length here either.
  if (UnboundPack(*Lhs, *sm) != nullptr) { return; }
  const auto lhs_ref = Lhs->InferTypeRef(sm, meta).WithoutConvention();
  if (type_compare::IsOpenTuple(lhs_ref, *sm->CurrentScope)) { return; }

  // Only a tuple or an array is indexed, within its bounds. The
  // lhs's type is only spelled out for the errors, held in a local:
  // "ERR_ARGS" keeps references, so a temporary would be freed
  // before the error reads it (a call's type is a fresh clone).
  if (not type_predicates::IsTypeCompTimeIndexable(lhs_ref, *sm->CurrentScope)) {
    const auto lhs_type = Lhs->InferType(sm, meta);
    Raise<SppMemberAccessNonIndexableError>(ERR_ARGS(*Lhs, *lhs_type, *TokDot));
  }
  if (const auto n = type_members::IndexBoundOf(lhs_ref, *sm->CurrentScope); std::stoul(Name->Val) >= n) {
    const auto lhs_type = Lhs->InferType(sm, meta);
    Raise<SppMemberAccessOutOfBoundsError>(ERR_ARGS(*Lhs, *lhs_type, n, *TokDot));
  }
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::_CheckNamedAccess(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  using member_lookup::MemberAccessForm;

  // A namespace is reached with "::", checked before the lhs's
  // type is read: a namespace has none.
  auto const *const lhs_ident = Lhs->To<IdentifierAst>();
  RaiseIf<SppMemberAccessStaticOperatorExpectedError>(
    lhs_ident != nullptr and sm->CurrentScope->FindVarSymbol(lhs_ident) == nullptr
      and sm->CurrentScope->FindNsSymbol(lhs_ident) != nullptr,
    ERR_ARGS(*Lhs, *TokDot, "namespace"));

  const auto lhs_ref = Lhs->InferTypeRef(sm, meta);
  auto const *const owner = lhs_ref.Symbol;
  if (owner == nullptr) { Ice(*Lhs, *sm->CurrentScope, "member access on a value whose type has no symbol"); }

  // Declared nowhere on the type (supers included): reached by
  // forwarding, analysed as the rewritten access in its place,
  // else an error - a "Try" member the "?" lowering reached for
  // means the operand is no "Try" type.
  const auto declaring = member_lookup::ScopesDeclaringVar(*owner->LinkedScope, *Name);
  if (declaring.empty()) {
    if (auto fwd_call = marker_sups::BuildFwdCall(*Lhs, lhs_ref, sm, meta, Name.get()); fwd_call != nullptr) {
      _MappedFwd = MakeShared<PostfixExpressionAst>(
        std::move(fwd_call), MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, Name));
      _MappedFwd->Stage7_AnalyseSemantics(sm, meta);
      return;
    }
    if (const auto try_op = AstAs<PostfixExpressionOperatorEarlyReturnAst>(Source.OriginalExpr); try_op != nullptr) {
      const auto lhs_type = Lhs->InferType(sm, meta);
      Raise<SppExpressionNotTryError>(ERR_ARGS(*try_op, *lhs_type));
    }
    member_lookup::RaiseMemberNotFound(*owner, lhs_ref, *Name, MemberAccessForm::Runtime, *sm->CurrentScope);
  }

  // The member, as "." reaches it, for every later read.
  _Owner = nullptr;
  ignore = _Resolve(sm, meta);

  // A method, visible through any block declaring it (not checked
  // for a lowering's own access).
  if (not Source.Lowered) {
    if (auto *const method = member_lookup::CheckMethodVisible(*owner->LinkedScope, *Name, *sm, *meta)) {
      lsp::resolution_index::RecordVariable(*Name, *sm, *meta, method);
    }
  }

  // An attribute. A "cmp" constant of the name belongs to the type,
  // so it is reached with "::": the form written is what picks an
  // attribute out where a constant shares its name.
  const auto members = declaring
    | genex::views::filter([](auto const &x) { return x.Symbol->Kind != VariableKind::FnMock; })
    | genex::to<Vec>();
  const auto runtime_members = member_lookup::MembersReachableBy(members, MemberAccessForm::Runtime);
  RaiseIf<SppMemberAccessStaticOperatorExpectedError>(
    runtime_members.empty() and not members.empty(),
    ERR_ARGS(*Name, *TokDot, "constant"));
  if (auto *const member = member_lookup::CheckClosestMember(
    member_lookup::ClosestScopes(runtime_members), *Name, *sm, *meta); member != nullptr) {
    lsp::resolution_index::RecordVariable(*Name, *sm, *meta, member);
  }
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;

  // A member reached by forwarding lives on the forwarded-to
  // value, so the mapped ast generates it: the forwarding call
  // it is applied to produces the borrow that is then indexed
  // into.
  if (_MappedFwd != nullptr) { return _MappedFwd->Stage11_CodeGen(sm, meta, ctx); }

  // This expression names storage, so it can produce either the
  // address of the field or the value held in it: assignment
  // targets and borrows want the address, every other context the
  // value.
  const auto want_address = meta->LlvmWantAddress;
  const auto uid = "." + Uid();
  const auto lhs_ref = Lhs->InferTypeRef(sm, meta);
  const auto base_ptr = _EmitBasePtr(lhs_ref, uid, sm, meta, ctx);

  // A field carrying no value is not laid out, so there is
  // nothing to index to and nothing to read: llvm has no value of
  // that type, no member for it in the struct, and "load void" is
  // not valid ir.
  const auto field_llvm_type = codegen::GetLlvmTypeOf(InferTypeRef(sm, meta).WithoutConvention(), ctx);
  if (codegen::IsValuelessType(field_llvm_type)) { return nullptr; }
  const auto field_ptr = _EmitFieldPtr(lhs_ref, base_ptr, uid, sm, ctx);
  if (want_address) { return field_ptr; }

  // Fields are never borrows (the second class borrow rules
  // forbid storing one), so the field's own lowered type is
  // always the type held in the slot.
  return ctx->Builder.CreateLoad(field_llvm_type, field_ptr, "member_access.field" + uid);
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::_EmitBasePtr(
  analyse::scopes::TypeRef const &lhs_ref, Str const &uid, ScopeManager *sm, CompilerMetaData *meta,
  codegen::LlvmCtx *ctx) -> llvm::Value* {
  // The address of the object the field is in: for "a.b.c", that
  // of "a.b". A borrow holds the address, so it is loaded out.
  const auto is_borrow = lhs_ref.IsBorrowed();
  const auto through_borrow = [&](llvm::Value *slot) -> llvm::Value* {
    return is_borrow
      ? ctx->Builder.CreateLoad(llvm::PointerType::get(*ctx->Context, 0), slot, "load.member_access.base_ptr" + uid)
      : slot;
  };
  const auto _meta_guard = MetaGuard(meta);

  // Another member access, asked for its field's address.
  if (IsRuntimeMemberAccess(Lhs)) {
    meta->LlvmWantAddress = true;
    return through_borrow(Lhs->Stage11_CodeGen(sm, meta, ctx));
  }
  meta->LlvmWantAddress = false;

  // A symbol: its storage is the object. A "cmp" constant is a
  // global, which belongs to the one module that defines it, so
  // another module reaches it through its own declaration.
  if (const auto sym = sm->CurrentScope->FindVarSymbolOutermost(*Lhs).first; sym != nullptr) {
    SPP_ASSERT(sym->LlvmInfo->Alloca != nullptr);
    auto object_ptr = sym->LlvmInfo->Alloca;
    if (const auto global_var = llvm::dyn_cast_or_null<llvm::GlobalVariable>(object_ptr); global_var != nullptr) {
      object_ptr = codegen::GetOrAddGlobalIntoCurrentModule(*global_var, *ctx->Module);
    }
    return through_borrow(object_ptr);
  }

  // A borrow made on the spot already is the object's address.
  if (is_borrow) { return Lhs->Stage11_CodeGen(sm, meta, ctx); }

  // Any other value is put in a temporary to index through: one
  // read off a temporary ("mk().a") drops the rest of it as its
  // statement ends.
  const auto llvm_type = lhs_ref.Symbol->LlvmInfo->LlvmType;
  SPP_ASSERT(llvm_type != nullptr);
  const auto temp = codegen::LlvmEntryAlloca(llvm_type, "temp.member_access.lhs" + uid, ctx);
  ctx->Builder.CreateStore(Lhs->Stage11_CodeGen(sm, meta, ctx), temp);
  if (DropsReceiverRest.has_value()) {
    codegen::PushTemporary(temp, lhs_ref.WithoutConvention(), *DropsReceiverRest, ctx);
  }
  return temp;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::_EmitFieldPtr(
  analyse::scopes::TypeRef const &lhs_ref, llvm::Value *base_ptr, Str const &uid, ScopeManager *sm,
  codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Indexed through the object's own type, not a borrow's pointer
  // type.
  IMPORT_UTILS;
  auto const &owner = *lhs_ref.Symbol;
  const auto llvm_type = owner.LlvmInfo->LlvmType;
  SPP_ASSERT(llvm_type != nullptr);

  // A named field is found through the type's field index map:
  // the S++ layout re-orders fields to minimise padding, so the
  // physical position is not the declaration's.
  if (not std::isdigit(Name->Val[0])) {
    const auto field_index = codegen::GetPhysicalFieldIndex(
      *owner.LlvmInfo, type_members::GetFieldIndexInType(owner, *Name));
    return ctx->Builder.CreateStructGEP(llvm_type, base_ptr, field_index, "member_access.field_ptr" + uid);
  }

  // An array lowers to "[n x T]", indexed through the array itself
  // (the leading zero steps over the pointer); a tuple to a struct
  // whose fields keep declaration order less its valueless elements,
  // so element "n" is found through the layout's index map.
  const auto index = static_cast<std::uint32_t>(std::stoul(Name->Val));
  if (type_predicates::IsTypeArray(TypeRef::ForKindCheck(owner, *sm->CurrentScope), *sm->CurrentScope)) {
    const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
    return ctx->Builder.CreateInBoundsGEP(
      llvm_type, base_ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)},
      "member_access.arr.elem_ptr" + uid);
  }
  return ctx->Builder.CreateStructGEP(
    llvm_type, base_ptr, codegen::GetPhysicalFieldIndex(*owner.LlvmInfo, index), "member_access.tup.elem_ptr" + uid);
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // A member reached by forwarding is resolved against the
  // forwarded-to value, which the rewritten access names.
  if (_MappedFwd != nullptr) {
    _MappedFwd->EvaluateCompTime(sm, meta);
    return;
  }

  // Resolve the left-hand-side expression.
  Lhs->EvaluateCompTime(sm, meta);

  // Numeric index access, of a tuple or an array.
  if (std::isdigit(Name->Val[0])) {
    auto const *const tup = meta->CompTimeResult->To<TupleLiteralAst>();
    auto const *const arr = meta->CompTimeResult->To<ArrayLiteralExplicitElementsAst>();
    if (tup != nullptr or arr != nullptr) {
      auto const &elems = tup != nullptr ? tup->Elems : arr->Elems;
      meta->CompTimeResult = AstClone(elems[std::stoul(Name->Val)]);
      return;
    }
  }

  // Handle normal attribute access (for objects).
  const auto cmp_obj = meta->CompTimeResult->To<ObjectInitializerAst>();
  meta->CompTimeResult = comp_time_intrinsics::GetCompTimeAttrValue(cmp_obj, Name.get());
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  //
  IMPORT_UTILS;

  // A member reached by forwarding belongs to the forwarded-to
  // type, so the rewritten access knows its type.
  if (_MappedFwd != nullptr) { return _MappedFwd->InferType(sm, meta); }

  // Named from its identity ("InferTypeRef"), which means
  // the same in any scope; the field's written type names
  // the owner's parameters.
  return InferTypeRef(sm, meta).AstIn(*sm->CurrentScope);
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // As "InferType": forwarded to the rewritten access, and
  // an element of a tuple or an array read off the left-hand
  // side's arguments; a named field is its type's symbol,
  // where the field's type resolves.
  IMPORT_UTILS;
  if (_MappedFwd != nullptr) { return _MappedFwd->InferTypeRef(sm, meta); }

  // Numeric index access (for tuples), or an element of a
  // pack not yet bound to its tuple, which is typed as its
  // element.
  if (std::isdigit(Name->Val[0])) {
    // An open pack's element is what the pack declares each to be, with its convention, read through the symbol it
    // names (so a "Self" in it is resolved here, not wherever a callee reads it); never off the open tuple, whose
    // element for a pack of one type is its hidden length parameter.
    if (auto const *const pack = UnboundPack(*Lhs, *sm); pack != nullptr) {
      auto const &element = pack->Pack->Element;
      auto const *const conv = element->GetConvention();
      return TypeRef::Of(
        *sm->CurrentScope->FindTypeSymbol(element.get())->AsBound(), *sm->CurrentScope,
        conv != nullptr ? Opt<ConventionTag>(conv->Tag()) : Nullopt);
    }
    const auto lhs_ref = Lhs->InferTypeRef(sm, meta).WithoutConvention();
    if (type_compare::IsOpenTuple(lhs_ref, *sm->CurrentScope)) {
      return type_compare::OpenTupleElement(lhs_ref, std::stoul(Name->Val), *sm->CurrentScope);
    }
    return type_members::GetNthTypeOfIndexableType(std::stoul(Name->Val), lhs_ref, *sm->CurrentScope);
  }

  // Read where the field is declared for this instance: its
  // owner's scope, which binds the parameters its type names.
  return _Resolve(sm, meta)->TypeRefIn(*_Owner->LinkedScope);
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::_Resolve(
  ScopeManager *sm, CompilerMetaData *meta) -> VariableSymbol* {
  //
  IMPORT_UTILS;
  if (_Owner != nullptr) { return _Member; }
  _Owner = Lhs->InferTypeRef(sm, meta).Symbol;
  _Member = member_lookup::MemberOf(*_Owner->LinkedScope, *Name, member_lookup::MemberAccessForm::Runtime);
  return _Member;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::GetFwdAccess() const -> PostfixExpressionAst* {
  return _MappedFwd.get();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::GetFwdReceiver() const -> PostfixExpressionAst* {
  // The lhs of the rewritten access is the forwarding call
  // ("x.fwd_ref()") applied to the original lhs.
  return _MappedFwd != nullptr ? _MappedFwd->Lhs->To<PostfixExpressionAst>() : nullptr;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::ExprParts() const -> Vec<IdentifierAst*> {
  return {Name.get()};
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::IsAllowedInDefault() const -> bool {
  // Reads what it is applied to, and holds nothing of its own.
  return true;
}

SPP_MOD_END
