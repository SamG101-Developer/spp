module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.packs;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lsp.resolution_index;
import spp.utils.functions;
import spp.utils.interner;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

namespace spp::asts {
  /// "PackElementCloneGuard": the closed packs whose elements a clone renames, by name, to their element counts.
  static thread_local auto pack_elements = spp::Map<spp::Str, std::size_t>();
}


SPP_MOD_BEGIN
spp::asts::PackElementCloneGuard::PackElementCloneGuard(
  Map<Str, std::size_t> packs) :
  _Packs(pack_elements, std::move(packs)) {
}

spp::asts::PostfixExpressionAst::PostfixExpressionAst(
  decltype(Lhs) &&lhs,
  decltype(Op) &&op) :
  Lhs(std::move(lhs)),
  Op(std::move(op)) {
  //
  Source.CachedInference = nullptr;
  if (Op != nullptr) { Op->Lhs = Lhs.get(); }
}

spp::asts::PostfixExpressionAst::~PostfixExpressionAst() = default;

auto spp::asts::PostfixExpressionAst::NewOverAnalysedLhs(
  decltype(Lhs) &&lhs, decltype(Op) &&op) -> Unique<PostfixExpressionAst> {
  // The lhs is a clone of an analysed operand, so it carries its
  // analysis; analysing it again would open its scopes twice.
  auto out = MakeUnique<PostfixExpressionAst>(std::move(lhs), std::move(op));
  out->_LhsAnalysed = true;
  return out;
}

auto spp::asts::PostfixExpressionAst::PosStart() const -> std::size_t {
  // Use the lhs.
  return Lhs->PosStart();
}

auto spp::asts::PostfixExpressionAst::PosEnd() const -> std::size_t {
  // Use the operator.
  return Op->PosEnd();
}

auto spp::asts::PostfixExpressionAst::Clone() const -> Unique<Ast> {
  // An element of a closed pack is cloned as the name of its parameter ("PackElementCloneGuard").
  if (auto const *const id = Lhs->To<IdentifierAst>(); id != nullptr and not pack_elements.empty()) {
    auto const *const access = Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
    const auto pack = pack_elements.find(id->Val);
    if (access != nullptr and pack != pack_elements.end()
      and genex::all_of(access->Name->Val, [](const char c) { return std::isdigit(c) != 0; })) {
      const auto index = std::stoul(access->Name->Val);
      if (index < pack->second) {
        return MakeUnique<IdentifierAst>(id->PosStart(), analyse::utils::packs::PackElementName(*id, index));
      }
    }
  }

  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionAst>(
    AstClone(Lhs),
    AstClone(Op));
  ast->_LhsAnalysed = _LhsAnalysed;
  return ast;
}

auto spp::asts::PostfixExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Lhs);
  SPP_STRING_APPEND(Op);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // An operator that rebuilds its operand ("?" into a "case")
  // get a copy of it as written, and owns every analysis of
  // it.
  if (Op->RebuildsOperand()) {
    const auto _meta_guard = MetaGuard(meta);
    Op->Lhs = Lhs.get();
    Op->WrittenOperand = AstClone(Lhs);
    Op->Stage7_AnalyseSemantics(sm, meta);
    return;
  }

  // A type left-hand side is moved into a shared pointer: a "TypeAst" needs "shared_from_this".
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ReturnTypeOverloadResolverType = nullptr;
    if (Lhs->To<TypeAst>() != nullptr) {
      auto temp_lhs = Shared<TypeAst>(Lhs.release()->As<TypeAst>());

      // A call's static receiver written without the arguments it needs ("Vec" in "Vec::from(&[1, 2])") is left for
      // the call, which has the arguments to infer it from ("InferTemplateReceiver"), and analyses this again.
      auto *const access = Op->To<PostfixExpressionOperatorStaticMemberAccessAst>();
      try {
        temp_lhs = type_resolution::AnalyseWrittenType(*temp_lhs, *sm, *meta);
      }
      catch (SppGenericParameterNotInferredError const &) {
        if (access == nullptr or not access->IsCalleeOfCall() or not temp_lhs->LastTypePart()->GnArgGroup->Args.empty()) {
          throw;
        }
        access->LeaveReceiverToCall(true);
        Lhs = AstClone(temp_lhs);
        return;
      }
      if (access != nullptr) { access->LeaveReceiverToCall(false); }
      Lhs = AstClone(temp_lhs); // Todo: std::move here once shared pointers are removed
    }
    else {
      // A deref under a member access is not a use of the whole
      // value: "b@.v" reads one field through the borrow and
      // "b@.v = 2" writes one, and neither copies nor moves what
      // "b" points at.
      meta->AllowMoveDeref = meta->AllowMoveDeref
        or Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr;

      // Standard analysis of the lhs (which is not a type), and
      // checking that the lhs is a valid form of primary expression.
      // An element of a pack ("xs.0") names the pack whole, as a spread does.
      if (not _LhsAnalysed) {
        // A static access called ("Vec::from(..)") may leave its receiver for the call (see above).
        if (auto const *const call = Op->To<PostfixExpressionOperatorFunctionCallAst>(); call != nullptr and call->Fold == nullptr) {
          if (auto *const callee = Lhs->To<PostfixExpressionAst>(); callee != nullptr) {
            if (auto *const callee_access = callee->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>()) {
              callee_access->MarkCalleeOfCall();
            }
          }
        }
        auto const *const access = Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
        const auto names_element = access != nullptr and std::isdigit(access->Name->Val[0]);
        const auto _pack_guard = spp::utils::functions::ScopedAssign(
          meta->AllowPackWhole, names_element and Lhs->To<IdentifierAst>() != nullptr);
        // A namespace may name the left of a member access: "::" reads it, and "." reports it (E: "::" expected).
        // Only that identifier: nothing analysed inside a left side ("foo(std).x", "[std].x") is one.
        const auto _ns_guard = spp::utils::functions::ScopedAssign(
          meta->AllowNamespaceName,
          (Op->To<PostfixExpressionOperatorStaticMemberAccessAst>() != nullptr or access != nullptr)
            and Lhs->To<IdentifierAst>() != nullptr);
        Lhs->Stage7_AnalyseSemantics(sm, meta);
        common_errors::RequirePrimary(*Lhs, *sm, {.AllowTypeAst = true});
      }
    }
  }

  // Re-attach the meta info, as it is targeting the lhs.
  const auto _meta_guard = MetaGuard(meta);
  Op->Lhs = Lhs.get();
  Op->Stage7_AnalyseSemantics(sm, meta);

  // Use the hook to record information for the resolution and
  // completion plugin.
  {
    const auto produces_value =
      Op->To<PostfixExpressionOperatorFunctionCallAst>() != nullptr or
      Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr;

    if (produces_value) {
      lsp::resolution_index::RecordExpression(
        *this, *sm, *meta, [this, sm, meta] { return InferType(sm, meta); });
    }
  }
}

auto spp::asts::PostfixExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // An operator that walks its operand itself checks it there: a
  // method call (or "x[i]", "x.await", "x.res()", which map to
  // one) as its "self" argument, rather than as a use of its own
  // here, and a "?" through the "case" it was rebuilt into.
  if (Op->WalksOperand()) {
    const auto _meta_guard = MetaGuard(meta);
    Op->Lhs = Lhs.get();
    Op->Stage8_CheckMemory(sm, meta);
    return;
  }

  // A member read off another member's value ("mk().a.b") leaves the inner read no end of a chain.
  auto const *const field = Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
  if (auto *const inner = field != nullptr ? _InnerFieldRead() : nullptr; inner != nullptr) {
    inner->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()->ReadOnwards = true;
  }

  // A dereference copies out of the borrow its lhs produced, so the value this expression is being assigned to holds
  // a copy and not the borrow. Any escaping borrow the lhs establishes on the way ("xs[i]@" resolving through the
  // "index_ref" coroutine) belongs to that temporary, not to the assignment target, so clear the target while the lhs
  // is checked - otherwise the target is recorded as containing a borrow it never receives, and nothing ever releases
  // it.
  {
    const auto _meta_guard = MetaGuard(meta);
    if (Op->To<PostfixExpressionOperatorDerefAst>() != nullptr) { meta->AssignmentTarget = nullptr; }
    if (Lhs != nullptr) { Lhs->Stage8_CheckMemory(sm, meta); }
  }

  if (field != nullptr and not field->ReadOnwards) { _TakeFieldsOffTemporary(*sm, meta); }

  const auto _meta_guard = MetaGuard(meta);
  Op->Lhs = Lhs.get();
  if (Lhs->To<IdentifierAst>() != nullptr) {
    // Validate the receiver is usable (not moved-out / inconsistent) before applying the operator, but do not treat
    // it as a move: accessing a member/deref/etc reads or borrows the receiver, it never consumes it.
    mem_utils::ValidateSymbolMemory(
      *Lhs, *Op, *sm, meta,
      {.CheckMove = false, .CheckPartialMove = false, .CheckMoveFromBorrowedCtx = false, .MarkMoves = false});
  }
  Op->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionAst::_InnerFieldRead() const -> PostfixExpressionAst* {
  auto *const inner = Lhs != nullptr ? Lhs->To<PostfixExpressionAst>() : nullptr;
  return inner != nullptr and inner->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr
    ? inner
    : nullptr;
}

auto spp::asts::PostfixExpressionAst::_TakeFieldsOffTemporary(
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // The fields the chain this read ends reads ("mk().a.b": "a", "b"), back
  // to the value it starts at. Only the first read holds a list of its own.
  auto path = Vec<regions::PlaceStep>();
  auto *first = this;
  for (auto *at = this; at != nullptr; at = at->_InnerFieldRead()) {
    auto *const read = at->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
    read->DropsReceiverRest = Nullopt;
    path.insert(path.begin(), regions::PlaceStep{.Name = read->Name->NameId()});
    first = at;
  }

  // Off a temporary, the first read makes it and drops the rest of it as the
  // statement ends: less the path if the value read here is moved out, all of
  // it if it is a copy or only borrowed ("ReadByBorrow"). The path leaves a hole in every value it runs through,
  // which is E106 for one with its own "drop", as for a named value.
  if (not drop_utils::IsTemporary(*first->Lhs, sm, meta)) { return; }

  // Reached by forwarding ("mk().n", "n" a field of what "mk()" forwards to), the read is through the borrow the
  // forwarding call hands back: nothing is taken off the temporary, which that call borrows as its "self", and drops
  // as any temporary argument it borrows.
  auto const *const first_read = first->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
  if (auto *const fwd_call = first_read->GetFwdReceiver(); fwd_call != nullptr) {
    if (auto const *const call = fwd_call->Op->To<PostfixExpressionOperatorFunctionCallAst>(); call != nullptr) {
      for (auto *const arg : call->FnArgGroup->GetKeywordArgs()) {
        if (arg->Name->Val == "self") { arg->DropsTemporary = true; }
      }
    }
    return;
  }
  auto &rest = first->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()->DropsReceiverRest;
  const auto read_by_borrow = Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()->ReadByBorrow;
  if (read_by_borrow or not InferTypeRef(&sm, meta).IsMoveOnly()) {
    rest = Vec<Vec<spp::utils::InternedId>>();
    return;
  }
  rest = Vec<Vec<spp::utils::InternedId>>{path | genex::views::transform(&regions::PlaceStep::Name) | genex::to<Vec>()};
  const auto field_name = Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()->Name;
  drop_utils::RaiseIfPathStrandsDestructor(
    first->Lhs->InferTypeRef(&sm, meta).WithoutConvention(), path, *field_name, *this, sm, meta);
}

auto spp::asts::PostfixExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Forward into the operator AST.
  const auto _meta_guard = MetaGuard(meta);
  Op->Lhs = Lhs.get();
  const auto ret_val = Op->Stage11_CodeGen(sm, meta, ctx);
  return ret_val;
}

auto spp::asts::PostfixExpressionAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward into the operator AST.
  const auto _meta_guard = MetaGuard(meta);
  Op->Lhs = Lhs.get();
  Op->EvaluateCompTime(sm, meta);
}

auto spp::asts::PostfixExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Check cache.
  // if (Source.CachedInference != nullptr) { return Source.CachedInference; }

  // Forward into the operator AST.
  const auto _meta_guard = MetaGuard(meta);
  Op->Lhs = Lhs.get();
  auto x = Op->InferType(sm, meta);
  return x;
}

auto spp::asts::PostfixExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  const auto _meta_guard = MetaGuard(meta);
  Op->Lhs = Lhs.get();
  return Op->InferTypeRef(sm, meta);
}

auto spp::asts::PostfixExpressionAst::ExprParts() const -> Vec<IdentifierAst*> {
  // Recursively search the lhs, and add the rhs if it
  // exists.
  auto lhs_parts = Lhs->ExprParts();
  auto rhs_parts = Op->ExprParts();
  if (not rhs_parts.empty()) {
    lhs_parts.append_range(genex::views::move(rhs_parts));
  }
  return lhs_parts;
}

auto spp::asts::PostfixExpressionAst::ReadExpr(
  ExprSubst const &sub) const -> Shared<ExpressionAst> {
  // The left-hand side is where a type is written - the
  // "A" of "A::new()", the "Self" of "Self::mo_seq_cst" -
  // and the operator carries whatever a call, an index or
  // a slice was given.
  return MakeShared<PostfixExpressionAst>(
    AstClone(Lhs->ReadExpr(sub)),
    Op->ReadExpr(sub));
}

auto spp::asts::PostfixExpressionAst::IsAllowedInDefault() const -> bool {
  // Check both the lhs and the postfix op on this ast.
  // Leaving the nullptr guards in because there is some
  // std::move(ast) for postfix iirc.
  return
    (Lhs == nullptr or Lhs->IsAllowedInDefault()) and
    (Op == nullptr or Op->IsAllowedInDefault());
}

SPP_MOD_END
