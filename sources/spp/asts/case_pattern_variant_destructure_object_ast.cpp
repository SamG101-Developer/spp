module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.assignment_utils;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
namespace spp::asts {
  namespace {
    /// A pattern can name a type narrower than any one alternative
    /// of its subject. For example, given a condition of the type
    /// "Opt[Opt[T]]", we need to allow "Some[Some[T]]" - ie inner
    /// variant narrowing.
    auto NarrowedLevel(
      TypeAst const &pattern, TypeAst const &alt,
      Scope const &scope) -> Pair<Shared<TypeAst>, Shared<TypeAst>> {
      using analyse::utils::type_compare::TypeEq;
      using analyse::utils::type_predicates::IsTypeVariant;

      const auto arg_at = [](TypeAst const &t, const std::size_t i) -> Shared<TypeAst> {
        auto const &args = t.LastTypePart()->GnArgGroup->Args;
        return i < args.Len() ? args[i]->TypeVal : nullptr;
      };

      for (auto i = 0uz; i < pattern.LastTypePart()->GnArgGroup->Args.Len(); ++i) {
        const auto p = arg_at(pattern, i);
        const auto a = arg_at(alt, i);
        // An argument can be absent on either side: the accessor above reads "TypeVal", which a comp argument
        // does not have, and the two argument lists need not be the same length.
        if (p == nullptr or a == nullptr) { continue; }

        // Check that a variant is being considered, and that
        // we don't have a direct (non-narrowing) match.
        if (not IsTypeVariant(TypeRef::OfHead(*a, scope), scope)) { continue; }
        if (TypeEq(*a, *p, scope, scope, false)) { continue; }
        if (codegen::GetVariantIndexOfMember(
          TypeRef::Of(*a, scope), TypeRef::Of(*p, scope), scope).has_value()) {
          return {p, a};
        }
      }
      return {nullptr, nullptr};
    }
  }
}

CasePatternVariantDestructureObjectAst::CasePatternVariantDestructureObjectAst(
  decltype(Type) type,
  decltype(TokL) &&tok_l,
  decltype(Elems) &&elems,
  decltype(TokR) &&tok_r) :
  Type(std::move(type)),
  TokL(std::move(tok_l)),
  Elems(std::move(elems)),
  TokR(std::move(tok_r)),
  _CondSym(nullptr),
  _FlowSym(nullptr) {
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}

CasePatternVariantDestructureObjectAst::~CasePatternVariantDestructureObjectAst() = default;

auto CasePatternVariantDestructureObjectAst::FromType(
  Shared<TypeAst> const &type) -> Unique<CasePatternVariantDestructureObjectAst> {
  // Build a destructure from a type, ie from "T" to make
  // "T()" for "case x of { T() { ... } }"
  auto empty_elems = Vec<Unique<CasePatternVariantAst>>{};
  return MakeUnique<CasePatternVariantDestructureObjectAst>(
    type, nullptr, std::move(empty_elems), nullptr);
}

auto CasePatternVariantDestructureObjectAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return Type->PosStart();
}

auto CasePatternVariantDestructureObjectAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR->PosEnd();
}

auto CasePatternVariantDestructureObjectAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto c = MakeUnique<CasePatternVariantDestructureObjectAst>(
    AstClone(Type),
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
  c->_MappedLet = AstClone(_MappedLet);
  return c;
}

auto CasePatternVariantDestructureObjectAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Type);
  SPP_STRING_APPEND(TokL);
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

auto CasePatternVariantDestructureObjectAst::BindsByMove() const -> bool {
  // A destructure binds if any of its elements does. An
  // empty one, or one made only of skips, is a shape test
  // and takes nothing.
  return genex::any_of(Elems, [](auto const &elem) { return elem->BindsByMove(); });
}

auto CasePatternVariantDestructureObjectAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::utils::type_predicates::IsTypeVariant;
  using analyse::utils::type_compare::TypeEq;
  using analyse::utils::type_utils::ResolveWrittenType;
  using analyse::errors::SppTypeMismatchError;

  // All factors type analysis.
  Type = ResolveWrittenType(*Type, *sm, *meta);

  // Handle "@" in the condition and move into it. Todo is
  // this still needed? It helps with the variant breakdown
  // within the deref type.
  auto *mapped_cond = meta->CaseCondition;
  if (analyse::utils::assignment_utils::IsDeref(mapped_cond)) {
    auto *const inner = mapped_cond->To<PostfixExpressionAst>()->Lhs.get();
    if (inner->To<IdentifierAst>() != nullptr) { mapped_cond = inner; }
  }

  // Flow-type the case condition (when it's a simple
  // identifier) so that both the eq-check expressions generated
  // by CreateAndAnalysePatternEqFuncs* and the member-access
  // bindings inside _MappedLet resolve against the narrowed
  // variant type (Pass[T] rather than the outer declared
  // type Res[T,E] for example).
  const auto cond_as_id = mapped_cond->To<IdentifierAst>();
  auto *const cond_sym = cond_as_id != nullptr ? sm->CurrentScope->GetVarSymbol(cond_as_id) : nullptr;
  _CondSym = cond_sym != nullptr ? cond_sym->SharedFromThis<VariableSymbol>() : nullptr;
  if (_CondSym != nullptr
    and IsTypeVariant(_CondSym->TypeRefIn(*sm->CurrentScope), *sm->CurrentScope)) {
    RaiseIf<SppTypeMismatchError>(
      not TypeEq(
        _CondSym->TypeRefIn(*sm->CurrentScope),
        TypeRef::Of(*Type, *sm->CurrentScope),
        *sm->CurrentScope, *sm->CurrentScope),
      {sm->CurrentScope}, ERR_ARGS(*meta->CaseCondition, *_CondSym->Type, *Type, *Type));
    _FlowSym = MakeShared<VariableSymbol>(*_CondSym);
    _FlowSym->LlvmInfo = _CondSym->LlvmInfo;

    // What this narrows, so that consuming through the
    // narrowed name discharges the value itself.
    _FlowSym->NarrowsSym = _CondSym;
    _FlowSym->Type = Type;
    _FlowSym->Kind = VariableKind::FlowNarrowing;

    if (Type->GetConvention() != nullptr) {
      const auto has_ast_scope = spp::get<1>(_CondSym->MemInfo->AstBorrowed);
      const auto borrow_scope = has_ast_scope ? has_ast_scope : _CondSym->ScopeDefinedIn;
      _FlowSym->MemInfo->AstBorrowed = {Type.get(), borrow_scope};
    }
    sm->CurrentScope->AddVarSymbol(_FlowSym);
  }

  AnalyseDestructure(mapped_cond, Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto CasePatternVariantDestructureObjectAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Snapshot the case condition memory info into the
  // flow symbol.
  if (_FlowSym != nullptr and _CondSym != nullptr) {
    _FlowSym->MemInfo->FillFromSnapshot(_CondSym->MemInfo->Snapshot());
  }

  // Forward memory checking to the mapped let statement.
  _MappedLet->Stage8_CheckMemory(sm, meta);
}

auto CasePatternVariantDestructureObjectAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // TODO: Do a non-variant type comparison first.
  // TODO: Do not allow if the condition type is variant.
  // Match when every element does.
  ResolveDestructure(Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto CasePatternVariantDestructureObjectAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Stupidly complex method but I think all parts are
  // covered now. Heavy documentation *READ IT ALL* when
  // making changes to this class.
  using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm;
  using analyse::utils::type_predicates::IsTypeVariant;
  using analyse::utils::type_compare::TypeEq;

  // A flow symbol only exists for a variant condition, whose
  // members live behind the discriminant, so the narrowed
  // bindings index from the payload buffer rather than from
  // the variant's base address.
  const auto uid = "." + spp::utils::Uid(this);
  auto llvm_tag_check = static_cast<llvm::Value*>(nullptr);
  if (_FlowSym and _CondSym) {
    // The subject's storage is read through the symbol the
    // scope holds now, not the one captured during analysis.
    // A second binding of the same name in the same scope
    // is a second symbol but still one table entry, and
    // storage is allocated onto whichever symbol that entry
    // names - so the captured one can be superseded and left
    // with none, which is what two "case" blocks over two
    // values both called "ip" used to crash on.
    if (_CondSym->LlvmInfo->Alloca == nullptr and _CondSym->ScopeDefinedIn != nullptr) {
      // The lookup goes in the scope the subject was declared
      // in, not the current one: this branch's own scope holds
      // the narrowed symbol under that same name, and it has
      // no storage of its own until further down.
      if (const auto live_sym = _CondSym->ScopeDefinedIn->GetVarSymbol(_CondSym->Name.get());
        live_sym != nullptr and live_sym->LlvmInfo->Alloca != nullptr) {
        _CondSym->LlvmInfo = live_sym->LlvmInfo;
      }
    }
    SPP_ASSERT(_CondSym->LlvmInfo->Alloca != nullptr);

    // Find the index in the variant's member types, of the
    // member type being flowed into. A variant can hold a borrow
    // as a member in its own right ("&S32 or None", which is what
    // resuming a "Gen[&S32]" gives), and then the convention is
    // part of what identifies the member rather than something
    // attached to the pattern, so the exact type is tried first.
    // Next, get the actual tag value from the variant that is
    // telling us which member type is active in the variant.
    auto variant_ptr = _CondSym->LlvmInfo->Alloca;
    if (_CondSym->Type->GetConvention() != nullptr) {
      variant_ptr = ctx->Builder.CreateLoad(
        llvm::PointerType::get(*ctx->Context, 0), variant_ptr, "case.pattern.subject" + uid);
    }

    // Each level of the pattern is checked in turn: its
    // discriminant against the subject it applies to, then the
    // payload that selected becomes the subject of the level
    // below. Checking only the outermost took every inner claim
    // on trust, so a "Some(None())" matched "Some[Some[T]]" and
    // the binding read the inner discriminant as if it were the
    // inner value. The walk also lands "current_ptr" on the
    // innermost payload, which is where the narrowed bindings
    // actually live.
    auto subject_type = _CondSym->Type;
    auto pattern_type = Type;
    auto current_ptr = variant_ptr;

    for (auto level = 0uz; ; ++level) {
      // Named per level, and that matters: several values are
      // emitted for each one, and this build of llvm does not
      // handle a repeated value name well - reusing a single
      // uid across the levels made the whole pattern miscompile,
      // non-deterministically.
      const auto level_uid = uid + "." + std::to_string(level);
      const auto llvm_subject_ty = sm->CurrentScope->GetTypeSymbol(
        subject_type->WithoutConvention().get())->LlvmInfo->LlvmType;
      SPP_ASSERT(llvm_subject_ty != nullptr);

      // A variant can hold a borrow as a member in its own
      // right ("&S32 or None", which is what resuming a
      // "Gen[&S32]" gives), and then the convention is part
      // of what identifies the member rather than something
      // attached to the pattern, so the exact type is tried
      // first.
      const auto subject_ref = TypeRef::Of(*subject_type, *sm->CurrentScope);
      const auto pattern_ref = TypeRef::Of(*pattern_type, *sm->CurrentScope);
      auto tag = codegen::GetVariantIndexOfMember(
        subject_ref, pattern_ref, *sm->CurrentScope);
      if (not tag.has_value()) {
        tag = codegen::GetVariantIndexOfMember(
          subject_ref, pattern_ref.WithoutConvention(), *sm->CurrentScope);
      }
      if (not tag.has_value()) { break; }

      const auto check = ctx->Builder.CreateICmpEQ(
        codegen::LoadVariantTag(current_ptr, llvm_subject_ty, "case.pattern.tag" + level_uid, ctx),
        llvm::ConstantInt::get(codegen::GetVariantTagType(ctx), *tag), "case.pattern.is" + level_uid);
      llvm_tag_check = llvm_tag_check == nullptr
        ? check
        : ctx->Builder.CreateAnd(llvm_tag_check, check, "case.pattern.is.all" + level_uid);

      current_ptr = codegen::GetVariantPayloadPtr(
        current_ptr, llvm_subject_ty, "case.pattern.payload" + level_uid, ctx);

      const auto alts = analyse::utils::type_compare::DedupVariableInnerTypes(
        *subject_type->WithoutConvention(), *sm->CurrentScope);
      if (*tag >= alts.Len()) { break; }

      auto [next_pattern, next_subject] = NarrowedLevel(*pattern_type, *alts[*tag], *sm->CurrentScope);
      if (next_pattern == nullptr) { break; }
      pattern_type = std::move(next_pattern);
      subject_type = std::move(next_subject);
    }

    // Set the alloca into the flow symbol (more precisely typed).
    // The flow symbol shares the condition symbol's llvm info up
    // to this point, so it is given its own here: writing the
    // payload address through the shared info would narrow the
    // condition symbol itself onto the payload for the remainder
    // of the enclosing function.
    _FlowSym->LlvmInfo = MakeShared<codegen::LlvmVarSymInfo>();
    _FlowSym->LlvmInfo->Alloca = current_ptr;
  }

  // A condition that is not a plain identifier has no symbol to
  // flow-type - "self@" is a deref, not a name - but its
  // discriminant still has to be checked.
  else if (meta->CaseCondition != nullptr and meta->LlvmCaseCondition != nullptr) {
    const auto cond_type = meta->CaseCondition->InferType(sm, meta);
    const auto bare_cond_type = cond_type != nullptr ? cond_type->WithoutConvention() : nullptr;

    if (bare_cond_type != nullptr
      and IsTypeVariant(TypeRef::OfHead(*bare_cond_type, *sm->CurrentScope), *sm->CurrentScope)) {
      const auto cond_ref = TypeRef::Of(*bare_cond_type, *sm->CurrentScope);
      const auto type_ref = TypeRef::Of(*Type, *sm->CurrentScope);
      auto tag = codegen::GetVariantIndexOfMember(cond_ref, type_ref, *sm->CurrentScope);
      if (not tag.has_value()) {
        tag = codegen::GetVariantIndexOfMember(cond_ref, type_ref.WithoutConvention(), *sm->CurrentScope);
      }

      // The condition was generated once by the enclosing "case",
      // so it is read rather than rebuilt: it may be the variant
      // value itself, or a pointer to it when the condition was
      // reached through a borrow.
      const auto llvm_variant_ty = sm->CurrentScope->GetTypeSymbol(
        bare_cond_type.get())->LlvmInfo->LlvmType;

      auto llvm_tag = static_cast<llvm::Value*>(nullptr);
      if (tag.has_value() and meta->LlvmCaseCondition->getType()->isPointerTy()) {
        llvm_tag = codegen::LoadVariantTag(
          meta->LlvmCaseCondition, llvm_variant_ty, "case.pattern.tag" + uid, ctx);
      }
      else if (tag.has_value() and meta->LlvmCaseCondition->getType() == llvm_variant_ty) {
        llvm_tag = ctx->Builder.CreateExtractValue(
          meta->LlvmCaseCondition, 0, "case.pattern.tag" + uid);
      }

      if (llvm_tag != nullptr and llvm_tag->getType() == codegen::GetVariantTagType(ctx)) {
        llvm_tag_check = ctx->Builder.CreateICmpEQ(
          llvm_tag, llvm::ConstantInt::get(codegen::GetVariantTagType(ctx), *tag),
          "case.pattern.is" + uid);
      }
    }
  }

  // Run the codegen on the transformed "let" ast to introduce
  // symbols into the llvm function.
  if (_MappedLet != nullptr) {
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->LetStatementPrecomputedValue = meta->LlvmCaseCondition;
      _MappedLet->Stage11_CodeGen(sm, meta, ctx);
    }
  }

  // Combine all the generated transforms into a single "AND"ed
  // expression.
  auto llvm_transforms = CreateAndAnalysePatternEqFuncsLlvm(
    Elems | genex::views::ptr | genex::to<Vec>(), sm, meta, ctx);

  const auto AND = [&ctx](auto a, auto b) { return ctx->Builder.CreateAnd(a, b); };
  auto llvm_master_transform = llvm_transforms.IsEmpty()
    ? llvm::cast<llvm::Value>(llvm::ConstantInt::getTrue(*ctx->Context))
    : genex::fold_left_first(llvm_transforms, std::move(AND));

  // Combine the potential variant check, should it exist, into
  // the type check. Place the tag check first (short-circuits
  // on the type check and prevents bad value comparisons).
  if (llvm_tag_check != nullptr) {
    llvm_master_transform = ctx->Builder.CreateAnd(
      llvm_tag_check, llvm_master_transform, "case.pattern.match" + uid);
  }

  return llvm_master_transform;
}

auto CasePatternVariantDestructureObjectAst::ConvToVar(
  CompilerMetaData *meta) -> Unique<LocalVariableAst> {
  // Recursively map the elements to their local variable
  // counterparts.
  auto mapped_elems = Elems
    | genex::views::transform([meta](auto const &x) { return x->ConvToVar(meta); })
    | genex::to<Vec>();

  // Create the final local variable wrapping, tag it and
  // return it.
  auto var = MakeUnique<LocalVariableDestructureObjectAst>(
    AstCloneShared(Type), AstClone(TokL), std::move(mapped_elems), AstClone(TokR));
  var->MarkFromCasePattern();
  return var;
}

SPP_MOD_END
