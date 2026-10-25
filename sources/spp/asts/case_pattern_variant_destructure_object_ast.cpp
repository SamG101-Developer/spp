module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.lex.tokens;
import spp.utils.ice;
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
      IMPORT_UTILS;

      const auto arg_at = [](TypeAst const &t, const std::size_t i) -> Shared<TypeAst> {
        auto const &args = t.LastTypePart()->GnArgGroup->Args;
        return i < args.size() ? args[i]->TypeVal : nullptr;
      };

      for (auto i = 0uz; i < pattern.LastTypePart()->GnArgGroup->Args.size(); ++i) {
        const auto p = arg_at(pattern, i);
        const auto a = arg_at(alt, i);
        // An argument can be absent on either side: the accessor above reads "TypeVal", which a comp argument
        // does not have, and the two argument lists need not be the same length.
        if (p == nullptr or a == nullptr) { continue; }

        // Check that a variant is being considered, and that
        // we don't have a direct (non-narrowing) match.
        if (not type_predicates::IsTypeVariant(TypeRef::ForKindCheck(*a, scope), scope)) { continue; }
        const auto a_ref = TypeRef::Of(*a, scope);
        const auto p_ref = TypeRef::Of(*p, scope);
        if (type_compare::TypeEq(a_ref, p_ref, scope, scope)) { continue; }
        if (codegen::GetVariantIndexOfMember(a_ref, p_ref, scope).has_value()) {
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
  _CondSymbol(nullptr),
  _FlowSymbol(nullptr) {
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
  IMPORT_UTILS;

  // All factors type analysis.
  Type = type_resolution::AnalyseWrittenType(*Type, *sm, *meta);

  // Handle "@" in the condition and move into it. Todo is
  // this still needed? It helps with the variant breakdown
  // within the deref type.
  auto *mapped_cond = meta->CaseCondition;
  if (IsDeref(mapped_cond)) {
    auto *const inner = mapped_cond->To<PostfixExpressionAst>()->Lhs.get();
    if (inner->To<IdentifierAst>() != nullptr) { mapped_cond = inner; }
  }

  // Flow-type the case condition (when it's a simple
  // identifier) so that the member-access bindings inside
  // _MappedLet, and the branch's body, resolve against the
  // narrowed variant type (Pass[T] rather than the outer
  // declared type Res[T,E] for example).
  const auto cond_as_id = mapped_cond->To<IdentifierAst>();
  auto *const cond_sym = cond_as_id != nullptr ? sm->CurrentScope->FindVarSymbol(cond_as_id) : nullptr;
  _CondSymbol = cond_sym != nullptr ? cond_sym->SharedFromThis<VariableSymbol>() : nullptr;
  if (_CondSymbol != nullptr
    and type_predicates::IsTypeVariant(_CondSymbol->TypeRefIn(*sm->CurrentScope), *sm->CurrentScope)) {
    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(
        _CondSymbol->TypeRefIn(*sm->CurrentScope),
        TypeRef::Of(*Type, *sm->CurrentScope),
        *sm->CurrentScope, *sm->CurrentScope),
      ERR_ARGS(*meta->CaseCondition, *_CondSymbol->Type, *Type, *Type));
    _FlowSymbol = MakeShared<VariableSymbol>(*_CondSymbol);
    _FlowSymbol->LlvmInfo = _CondSymbol->LlvmInfo;

    // What this narrows, so that consuming through the
    // narrowed name discharges the value itself.
    _FlowSymbol->NarrowsSymbol = _CondSymbol;
    _FlowSymbol->Type = Type;
    _FlowSymbol->Kind = VariableKind::FlowNarrowing;

    if (Type->GetConvention() != nullptr) { _FlowSymbol->MemInfo->AstBorrowed = Type.get(); }
    _DeclareFlowSymbol(*sm);
  }

  AnalyseDestructure(mapped_cond, Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto CasePatternVariantDestructureObjectAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Snapshot the case condition memory info into the
  // flow symbol.
  if (_FlowSymbol != nullptr and _CondSymbol != nullptr) {
    auto cond_state = memory_state::StateOf(*sm->CurrentScope, *_CondSymbol);
    memory_state::StateOf(*sm->CurrentScope, *_FlowSymbol) = std::move(cond_state);
  }

  // The narrowed subject is declared here, before the
  // bindings (which may reuse its name: "is Some(val)"
  // on "val").
  _DeclareFlowSymbol(*sm);

  // Forward memory checking to the mapped let statement.
  _MappedLet->Stage8_CheckMemory(sm, meta);
}

auto CasePatternVariantDestructureObjectAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // The narrowed subject holds the subject's comptime value: it
  // was copied at stage 7, before the subject had one, and the
  // bindings read through it. The subject is read where it was
  // declared, as this branch's scope holds the narrowed one.
  if (_FlowSymbol != nullptr and _CondSymbol != nullptr and _CondSymbol->ScopeDefinedIn != nullptr) {
    const auto live_sym = _CondSymbol->ScopeDefinedIn->FindVarSymbol(_CondSymbol->Name.get());
    const auto &value = live_sym != nullptr ? live_sym->CompTimeValue : _CondSymbol->CompTimeValue;
    _FlowSymbol->CompTimeValue = AstClone(value);
  }

  // Resolve the mapped "let"; the "case"'s plan checks the tags
  // and the literals.
  _DeclareFlowSymbol(*sm);
  CompTimeResolveDestructure(sm, meta);
}

auto CasePatternVariantDestructureObjectAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Whether the subject holds what this pattern names is the
  // "case"'s plan's to check ("PatternSolver"); this points the
  // narrowed subject at the member's payload, where the bindings
  // read from, and introduces the bindings.
  IMPORT_UTILS_AND_UID;

  // A flow symbol only exists for a variant condition, whose
  // members live behind the discriminant, so the narrowed
  // bindings index from the payload buffer rather than from
  // the variant's base address.
  const auto uid = "." + Uid();
  if (_FlowSymbol and _CondSymbol) {
    // The subject's storage is read through the symbol the
    // scope holds now, not the one captured during analysis.
    // A second binding of the same name in the same scope
    // is a second symbol but still one table entry, and
    // storage is allocated onto whichever symbol that entry
    // names - so the captured one can be superseded and left
    // with none, which is what two "case" blocks over two
    // values both called "ip" used to crash on.
    if (_CondSymbol->LlvmInfo->Alloca == nullptr and _CondSymbol->ScopeDefinedIn != nullptr) {
      // The lookup goes in the scope the subject was declared
      // in, not the current one: this branch's own scope holds
      // the narrowed symbol under that same name, and it has
      // no storage of its own until further down.
      if (const auto live_sym = _CondSymbol->ScopeDefinedIn->FindVarSymbol(_CondSymbol->Name.get());
        live_sym != nullptr and live_sym->LlvmInfo->Alloca != nullptr) {
        _CondSymbol->LlvmInfo = live_sym->LlvmInfo;
      }
    }
    SPP_ASSERT(_CondSymbol->LlvmInfo->Alloca != nullptr);

    // A borrowed subject's storage holds the address of the
    // variant rather than the variant.
    auto variant_ptr = _CondSymbol->LlvmInfo->Alloca;
    if (_CondSymbol->Type->GetConvention() != nullptr) {
      variant_ptr = ctx->Builder.CreateLoad(
        llvm::PointerType::get(*ctx->Context, 0), variant_ptr, "case.pattern.subject" + uid);
    }

    // Each level of the pattern selects a member of the subject
    // it applies to, whose payload is the subject of the level
    // below ("Some[Some[T]]" on "Opt[Opt[T]]"). The walk lands on
    // the innermost payload, which is where the narrowed bindings
    // live.
    auto subject_type = _CondSymbol->Type;
    auto pattern_type = Type;
    auto current_ptr = variant_ptr;

    for (auto level = 0uz; ; ++level) {
      // Named per level, and that matters: several values are
      // emitted for each one, and this build of llvm does not
      // handle a repeated value name well - reusing a single
      // uid across the levels made the whole pattern miscompile,
      // non-deterministically.
      const auto level_uid = uid + "." + std::to_string(level);
      const auto llvm_subject_ty = sm->CurrentScope->FindTypeSymbol(
        subject_type->WithoutConvention().get())->LlvmInfo->LlvmType;
      SPP_ASSERT(llvm_subject_ty != nullptr);

      // A variant can hold a borrow as a member in its own
      // right ("&S32 or GenDone", which is what resuming a
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

      // A pattern naming several of the subject's members at once
      // ("is Opt[S32](..)" on "Some[S32] or None or GenDone") is
      // not one member, so it binds the value re-tagged as that
      // narrower variant.
      if (not tag.has_value()) {
        const auto [is_member, narrowed] = codegen::NarrowVariant(
          current_ptr, TypeRef::Of(*subject_type->WithoutConvention(), *sm->CurrentScope),
          pattern_ref.WithoutConvention(), *sm->CurrentScope, "case.pattern.narrow" + level_uid, ctx);
        if (is_member != nullptr) { current_ptr = narrowed; }
        break;
      }

      current_ptr = codegen::GetVariantPayloadPtr(
        current_ptr, llvm_subject_ty, "case.pattern.payload" + level_uid, ctx);

      const auto alts = type_members::VariantMembers(
        *subject_type->WithoutConvention(), *sm->CurrentScope);
      if (*tag >= alts.size()) { break; }

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
    _FlowSymbol->LlvmInfo = MakeShared<codegen::LlvmVarSymbolInfo>();
    _FlowSymbol->LlvmInfo->Alloca = current_ptr;
    _DeclareFlowSymbol(*sm);
  }

  // Run the codegen on the transformed "let" ast to introduce
  // symbols into the llvm function.
  if (_MappedLet != nullptr) {
    const auto _meta_guard = MetaGuard(meta);
    meta->LetStatementPrecomputedValue = meta->LlvmCaseCondition;
    _MappedLet->Stage11_CodeGen(sm, meta, ctx);
  }
  return llvm::ConstantInt::getTrue(*ctx->Context);
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

auto CasePatternVariantDestructureObjectAst::_DeclareFlowSymbol(
  ScopeManager &sm) const -> void {
  // Every walk declares the narrowed subject where the pattern
  // is, as Stage 7 did: a binding of the same name after it
  // replaced it in the table, and the bindings' own values read
  // it (cleaned by scope endings).
  if (_FlowSymbol != nullptr) {
    sm.CurrentScope->AddVarSymbol(_FlowSymbol);
  }
}

SPP_MOD_END
