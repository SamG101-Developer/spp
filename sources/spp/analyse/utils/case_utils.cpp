module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.case_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.utils.ptr;
import spp.utils.uid;
import genex;
import std;

namespace spp::analyse::utils::case_utils {
  namespace {
    /// The value of one field of an already-generated aggregate,
    /// indexed rather than rebuilt.
    auto NarrowOntoField(
      TypeAst const &base_type, IdentifierAst const &field_name,
      llvm::Value *const llvm_base, ScopeManager const &sm,
      LlvmCtx *const ctx) -> llvm::Value* {
      using type_members::GetFieldIndexInType;
      using type_predicates::IsTypeArr;

      const auto uid = "." + spp::utils::Uid(&field_name);
      const auto bare_type = base_type.WithoutConvention();
      const auto base_type_sym = sm.CurrentScope->GetTypeSymbol(bare_type.get());
      if (base_type_sym == nullptr or base_type_sym->LlvmInfo->LlvmType == nullptr) { return nullptr; }
      const auto llvm_base_ty = base_type_sym->LlvmInfo->LlvmType;

      // Indexing needs an address. A borrowed subject already
      // is one; anything else is a value, and gets a slot to
      // be indexed through - the same materialisation the member
      // access does for a non-symbolic base.
      auto base_ptr = llvm_base;
      if (not llvm_base->getType()->isPointerTy()) {
        base_ptr = LlvmEntryAlloca(llvm_base_ty, "case.pattern.subject" + uid, ctx);
        ctx->Builder.CreateStore(llvm_base, base_ptr);
      }

      // Determine the field pointer which can be set via the
      // runtime member access field (numeric or identifier).
      auto field_ptr = static_cast<llvm::Value*>(nullptr);

      // For the numeric case, we need to either GEP into the
      // array target, or struct GEP into the tuple target.
      if (std::isdigit(static_cast<unsigned char>(field_name.Val[0]))) {
        const auto index = static_cast<std::uint32_t>(std::stoul(field_name.Val));

        // An array lowers to "[n x T]" rather than to a struct,
        // so it is indexed through the array itself.
        if (IsTypeArr(*base_type_sym, *sm.CurrentScope)) {
          const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
          field_ptr = ctx->Builder.CreateGEP(
            llvm_base_ty, base_ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)},
            "case.pattern.elem_ptr" + uid);
        }

        // Tuples use the normal struct GEP function, which
        // gets the target field by internal pointer math. No
        // mapping because tuples' field order matches the
        // type arguments' order.
        else {
          field_ptr = ctx->Builder.CreateStructGEP(
            llvm_base_ty, base_ptr, index,
            "case.pattern.elem_ptr" + uid);
        }
      }

      // The layout re-orders fields to minimize padding, so a
      // named attribute's declaration index has to be resolved
      // through the type's own field index map.
      else {
        const auto decl_index = GetFieldIndexInType(*base_type_sym, field_name);
        const auto field_index = GetPhysicalFieldIndex(*base_type_sym->LlvmInfo, decl_index);
        field_ptr = ctx->Builder.CreateStructGEP(
          llvm_base_ty, base_ptr, field_index, "case.pattern.field_ptr" + uid);
      }

      return field_ptr;
    }

    /// Compare two escaping-borrow container lists by the memory
    /// regions they each name. Each branch of a "case" builds
    /// its own ast nodes, so the same borrow written in two
    /// branches is two pointers but one region, and only the
    /// region is that makes the branches disagree or agree.
    /// Todo: Verify this.
    auto EscapingBorrowContainersDiffer(
      Vec<Tup<Ast const*, Ast const*>> const &lhs,
      Vec<Tup<Ast const*, Ast const*>> const &rhs)
      -> bool {
      // The region converter takes the escaping borrows lists
      // and stringifies them, then sorts and compares.
      const auto regions = [](auto const &list) {
        auto out = Vec<Str>();
        for (auto const &[container, borrow] : list) {
          out.EmplaceBack(container->ToString() + " <- " + borrow->ToString());
        }
        genex::actions::sort(out);
        return out;
      };
      return regions(lhs) != regions(rhs);
    }

    /// Build the "cond.<field>.eq(&literal)" for a literal
    /// element of a pattern, analyse it where the pattern is
    /// (and walk back the scope as the condition might
    /// introduce scopes).
    template <typename T>
    auto MapFieldEqLiteral(
      Unique<PostfixExpressionOperatorRuntimeMemberAccessAst> &&field,
      ExpressionAst const &literal, ScopeManager *sm, CompilerMetaData *meta,
      Function<T(Ast *)> const &mapper) -> T {
      // Build the asts up.
      auto pf_expr = MakeUnique<PostfixExpressionAst>(
        AstClone(meta->CaseCondition), std::move(field));
      auto eq_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
        MakeUnique<ConventionRefAst>(nullptr), nullptr, AstClone(&literal));
      auto eq_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
        nullptr, MakeShared<IdentifierAst>(0uz, "eq"));
      auto eq_pf_expr = MakeUnique<PostfixExpressionAst>(
        std::move(pf_expr), std::move(eq_field));
      auto eq_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
        nullptr, nullptr, nullptr);
      eq_call->FnArgGroup->Args.EmplaceBack(std::move(eq_arg));
      const auto eq_call_expr = MakeUnique<PostfixExpressionAst>(
        std::move(eq_pf_expr), std::move(eq_call));

      // Analyse and walk back the scope.
      const auto current_scope = sm->CurrentScope;
      const auto current_scope_iter = sm->CurrentIterator();
      eq_call_expr->Stage7_AnalyseSemantics(sm, meta);
      sm->Reset(current_scope, current_scope_iter);
      return mapper(eq_call_expr.get());
    }

    template <typename T>
    auto CreateAndAnalysePatternEqFuncsCore(
      Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
      CompilerMetaData *meta, Function<T(Ast *)> &&mapper,
      Function<void(ExpressionAst *)> &&on_nested_subject = {})
      -> Vec<T> {
      auto transformed = Vec<T>();
      transformed.reserve(elems.Len());

      if (not elems.IsEmpty() and elems[0]->To<CasePatternVariantExpressionAst>()) {
        // For expression patterns, just generate the expression once.
        const auto expr_part = elems[0]->To<CasePatternVariantExpressionAst>();
        auto transform = mapper(expr_part->Expr.get());
        transformed.EmplaceBack(std::move(transform));
        return transformed;
      }

      // A bound-or-unbound multi-argument skip ("..") absorbs a number
      // of real array/tuple slots that isn't known until the real (rhs)
      // length is known, so any positionally-addressed element after it
      // ("cond.<i>") cannot use its raw position within "elems" - that
      // would under-count by the width of the skip and read one slot too
      // early.
      auto skip_index = std::optional<std::size_t>{};
      for (auto const &[i, part] : elems | genex::views::enumerate) {
        if (part->To<CasePatternVariantDestructureSkipMultipleArgumentsAst>() != nullptr) {
          skip_index = i;
          break;
        }
      }

      // Todo: move "max length" into type_utils function, and route the
      //  "is in bounds" through that too.
      auto num_rhs_elems = std::optional<std::size_t>{};
      const auto real_index = [&](const std::size_t i) -> std::size_t {
        if (not skip_index.has_value() or i <= *skip_index) { return i; }
        if (not num_rhs_elems.has_value()) {
          const auto cond_type = meta->CaseCondition->InferType(sm, meta);
          const auto &gn_arg_group = cond_type->LastTypePart()->GnArgGroup;
          const auto cond_ref = TypeRef::OfHead(*cond_type, *sm->CurrentScope);
          num_rhs_elems = type_predicates::IsTypeArr(cond_ref, *sm->CurrentScope)
            ? std::stoull(
              gn_arg_group->Args[1]->CompVal->ToUnchecked<IntegerLiteralAst>()->Val->TokenData)
            : gn_arg_group->Args.Len();
        }
        return *num_rhs_elems - (elems.Len() - i);
      };

      for (auto const &[i, part] : elems | genex::views::enumerate) {
        // For literals and expressions, generate the equality checks.
        if (auto const *const literal = part->To<CasePatternVariantLiteralAst>(); literal != nullptr) {
          // The element by its position: "cond.<i>.eq(&literal)".
          auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
            nullptr, MakeShared<IdentifierAst>(0uz, std::to_string(real_index(i))));
          transformed.EmplaceBack(MapFieldEqLiteral(
            std::move(field), *literal->Literal->To<ExpressionAst>(), sm, meta, mapper));
        }

        // For named attribute bindings whose value is a literal, like "field=literal".
        else if (const auto cast_attr = part->To<CasePatternVariantDestructureAttributeBindingAst>();
          cast_attr != nullptr and cast_attr->Val->To<CasePatternVariantLiteralAst>() != nullptr) {
          // The attribute by its name: "cond.field.eq(&literal)".
          const auto literal_part = cast_attr->Val->To<CasePatternVariantLiteralAst>();
          auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
            nullptr, AstCloneShared(cast_attr->Name));
          transformed.EmplaceBack(MapFieldEqLiteral(
            std::move(field), *literal_part->Literal->To<ExpressionAst>(), sm, meta, mapper));
        }

        // For nested objects (array, tuple, object)
        else if (
          part->To<CasePatternVariantDestructureArrayAst>() != nullptr or
          part->To<CasePatternVariantDestructureTupleAst>() != nullptr or
          part->To<CasePatternVariantDestructureObjectAst>() != nullptr) {
          // Generate the extraction on the condition for this part, like "cond.0".
          auto field_name = MakeShared<IdentifierAst>(0uz, std::to_string(real_index(i)));
          auto field = MakeUnique<
            PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
          auto pf_expr = MakeUnique<PostfixExpressionAst>(AstClone(meta->CaseCondition), std::move(field));

          // Update the "meta->cond" with the "pf_expr", and analyse against the inner part.
          const auto _meta_guard = MetaGuard(meta);
          meta->CaseCondition = pf_expr.get();
          if (on_nested_subject) { on_nested_subject(pf_expr.get()); }

          // Combine the result.
          auto transform = mapper(part);
          transformed.EmplaceBack(std::move(transform));
        }
      }

      return transformed;
    }
  }
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta, LlvmCtx *ctx) -> Vec<llvm::Value*> {
  // Get the expression and map then to LLVM values.
  Function<llvm::Value*(Ast *)> map = [&](Ast *x) {
    return x->Stage11_CodeGen(sm, meta, ctx);
  };

  // Narrow the subject's llvm value onto each element alongside
  // its ast. The element access is built fresh here, so it has
  // to be analysed before it can be generated - the literal
  // branches above do the same, saving and restoring the scope
  // around it because analysis walks into the condition's own scope.
  Function<void(ExpressionAst *)> on_nested_subject = [&](ExpressionAst *subject) {
    // Analyse the subject, and then walk back the scope iterator,
    // as the value itself might have introduced new scopes.
    const auto current_scope = sm->CurrentScope;
    const auto current_scope_iter = sm->CurrentIterator();
    subject->Stage7_AnalyseSemantics(sm, meta);
    sm->Reset(current_scope, current_scope_iter);

    // If the subject is a postfix expression, extract the runtime
    // member access as the operator (otherwise nullptr).
    const auto access = subject->To<PostfixExpressionAst>();
    const auto field = access != nullptr
      ? access->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()
      : nullptr;

    // Anything that is not the plain field access requires code
    // generation. No more work needs to be done after that.
    if (field == nullptr or meta->LlvmCaseCondition == nullptr) {
      meta->LlvmCaseCondition = subject->Stage11_CodeGen(sm, meta, ctx);
      return;
    }

    // For nested variant destructures, we need to narrow the
    // value into the target type field, providing access onto
    // the narrowed-type value.
    const auto base_type = access->Lhs->InferType(sm, meta);
    const auto field_ptr = NarrowOntoField(
      *base_type, *field->Name, meta->LlvmCaseCondition, *sm, ctx);
    if (field_ptr == nullptr) {
      meta->LlvmCaseCondition = subject->Stage11_CodeGen(sm, meta, ctx);
      return;
    }

    // A field carrying no value is not laid out, so there is
    // nothing to read and "load void" is not valid ir.
    const auto field_llvm_ty = subject->InferTypeRef(sm, meta).Sym->LlvmInfo->LlvmType;
    meta->LlvmCaseCondition = IsValuelessType(field_llvm_ty)
      ? nullptr
      : ctx->Builder.CreateLoad(field_llvm_ty, field_ptr, "case.pattern.subject.value");
  };

  // Forward the nested analysis lambda into the core checker
  // to propagate the nested checks properly.
  auto asts = CreateAndAnalysePatternEqFuncsCore(
    elems, sm, meta, std::move(map), std::move(on_nested_subject));
  return asts;
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta) -> Vec<Unique<ExpressionAst>> {
  // Get the expression and map then to Comptime values.
  Function<Unique<ExpressionAst>(Ast *)> map = [&](Ast *x) {
    x->Stage9_CompTimeResolve(sm, meta);
    return std::move(meta->CmpResult);
  };

  auto asts = CreateAndAnalysePatternEqFuncsCore(elems, sm, meta, std::move(map));
  return asts;
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsDummyCore(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta) -> void {
  //
  Function<std::monostate(Ast *)> noop = [](Ast *) { return std::monostate{}; };
  CreateAndAnalysePatternEqFuncsCore(elems, sm, meta, std::move(noop));
}

auto spp::analyse::utils::case_utils::ValidateInconsistentTypes(
  Vec<CaseExpressionBranchAst*> const &branches, ScopeManager &sm,
  CompilerMetaData *meta) -> Tup<Pair<Ast*, Shared<TypeAst>>, Vec<Pair<Ast*, Shared<TypeAst>>>> {
  //
  using errors::SppTypeMismatchError;

  // Collect type information for each branch, pairing the
  // branch with its inferred type.
  auto branches_type_info = branches
    | genex::views::transform([&sm, meta](auto *x) { return MakePair(x, x->InferType(&sm, meta)); })
    | genex::to<Vec>();

  // The valued branches are branches that are non-terminating.
  // This is because "ret" from a branch doesn't pass a value
  // back to the binding, so shouldn't be considered for type
  // checking.
  auto valued_branches_type_info = branches_type_info
    | genex::views::remove_if([](auto const &x) { return x.first->Body->Terminates(); })
    | genex::to<Vec>();
  if (valued_branches_type_info.IsEmpty()) { valued_branches_type_info = branches_type_info; }

  // Filter the branch types down to variant types for custom
  // analysis.
  auto variant_branches_type_info = valued_branches_type_info
    | genex::views::filter([&sm](auto &&x) {
      return type_predicates::IsTypeVariant(TypeRef::OfHead(*x.second, *sm.CurrentScope), *sm.CurrentScope);
    })
    | genex::to<Vec>();

  // Set the master branch type to the first branch that has a
  // value: a branch that diverges ("!") has none for the others
  // to agree with, and only a case whose every branch diverges
  // is "!" itself. This is the default and may be subsequently
  // changed. Override it if an assignment type is given.
  auto master_branch_type_info = MakePair<CaseExpressionBranchAst*, Shared<TypeAst>>(nullptr, nullptr);
  for (auto const &[branch, type] : valued_branches_type_info) {
    if (not type->IsNeverType()) {
      master_branch_type_info = MakePair(branch, type);
      break;
    }
  }
  if (master_branch_type_info.first == nullptr and not valued_branches_type_info.IsEmpty()) {
    master_branch_type_info = MakePair(valued_branches_type_info[0].first, valued_branches_type_info[0].second);
  }
  if (meta->AssignmentTargetType != nullptr) {
    master_branch_type_info = MakePair(nullptr, meta->AssignmentTargetType);
  }

  // Otherwise, if there are variant branches, use the most
  // variant type as the master branch type.
  else if (not variant_branches_type_info.IsEmpty()) {
    auto most_inner_types = 0uz;
    for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
      const auto variant_size = type_compare::VariantMembers(
        TypeRef::Of(*variant_type, *sm.CurrentScope), *sm.CurrentScope).Len();
      if (variant_size > most_inner_types) {
        master_branch_type_info = {variant_branch, variant_type};
        most_inner_types = variant_size;
      }
    }
  }

  // Remove the master branch pointer from the list of remaining
  // branch types and check all types match. A "!" branch fits
  // the master type like any value would, so "TypeEq" drops it.
  auto mismatch_branches_type_info = valued_branches_type_info
    | genex::views::remove_if([&](auto const &x) {
      return x.first == master_branch_type_info.first;
    })
    | genex::views::remove_if([&](auto const &x) {
      return type_compare::TypeEq(*master_branch_type_info.second, *x.second, *sm.CurrentScope, *sm.CurrentScope);
    })
    | genex::to<Vec>();

  if (not mismatch_branches_type_info.IsEmpty()) {
    const auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
    const auto [master_branch, master_branch_type] = master_branch_type_info;
    const auto final_member = master_branch ? master_branch->Body->FinalMember() : meta->AssignmentTarget.get();
    Raise<SppTypeMismatchError>(
      {sm.CurrentScope},
      ERR_ARGS(*final_member, *master_branch_type, *mismatch_branch->Body->FinalMember(), *mismatch_branch_type));
  }

  // The `master_branch_type_info.first` is deliberately null when an
  // assignment target type drove the master type (see above); calling
  // `To<>()` through that null pointer is UB, so guard it and keep
  // the null.
  const auto cast_master_branch_type_info = MakePair(
    master_branch_type_info.first ? master_branch_type_info.first->template ToUnchecked<Ast>() : nullptr,
    master_branch_type_info.second);

  // Cast to common AST nodes and return with the types.
  const auto cast_branches_type_info = branches_type_info
    | genex::views::transform([](auto &&x) {
      return MakePair(x.first->template ToUnchecked<Ast>(), x.second);
    })
    | genex::to<Vec>();
  return {cast_master_branch_type_info, cast_branches_type_info};
}

auto spp::analyse::utils::case_utils::ValidateInconsistentMemory(
  Ast *parent, Vec<CaseExpressionBranchAst*> const &branches,
  VariableSymbol *const subject, ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Define a simple alias for a list of symbols and their
  // memory.
  using SymbolMemoryList = Vec<Pair<CaseExpressionBranchAst*, mem_info_utils::MemoryInfoSnapshot>>;
  using SymbolMemoryMap = Map<VariableSymbol*, mem_info_utils::MemoryInfoSnapshot>;

  // Create a map of the symbols' memory  information before
  // any branches are analysed.
  auto sym_mem_info = Map<VariableSymbol*, SymbolMemoryList>();

  // The lookup walks ancestors and super scopes, which can
  // reach one symbol by more than one route, and every list
  // below is built with one entry per branch per occurrence.
  // Deduplicate.
  auto vs = Vec<VariableSymbol*>();
  auto seen_syms = Set<VariableSymbol*>();
  for (auto *sym : sm->CurrentScope->AllVarSymbols()) {
    if (seen_syms.insert(sym).second) { vs.EmplaceBack(sym); }
  }

  // The states before any branch has run. Each branch is restored
  // to these before the next one is analysed, and they stand in
  // as a final pseudo-branch for the consistency comparison below
  // - the same snapshot serving both, since nothing between the
  // two uses moves them apart.
  auto pre_analysis_mem_info = vs
    | genex::views::transform([](auto const &x) { return MakePair(x, x->MemInfo->Snapshot()); })
    | genex::to<Vec>();

  for (auto &&branch : branches) {
    // Analyse the memory and then recheck the symbols' memory
    // status.
    branch->Stage8_CheckMemory(sm, meta);

    // A branch binding parts off the subject takes the whole
    // of it - the "case" marks the subject moved once every
    // branch has run - so a part this branch left unbound is
    // a part nothing holds. Check all movable fields have been
    // bound, so dropping can take place.
    const auto branch_binds = subject != nullptr and genex::any_of(
      branch->Patterns, [](auto const &pattern) { return pattern->BindsByMove(); });
    if (branch_binds) {
      if (const auto skipped = linear_utils::FirstUnaccountedPart(
        *subject, Vec<IdentifierAst*>{subject->Name.get()}, *sm); not skipped.empty()) {
        auto const *const blamed = branch->Patterns.IsEmpty()
          ? static_cast<Ast const*>(branch)
          : static_cast<Ast const*>(branch->Patterns[0].get());

        Raise<errors::SppDestructureSkipsOwnedPartError>(
          {sm->CurrentScope}, ERR_ARGS(*blamed, *subject->Name, StrView(skipped)));
      }
    }

    auto new_symbol_mem_info = vs
      | genex::views::transform([](auto const &x) { return MakePair(x, x->MemInfo->Snapshot()); })
      | genex::to<Vec>();

    // Reset the memory status of the symbols for the next branch
    // to analyse with the same original memory states.
    // Todo: Scopes need restoring properly too. (And rename to AstInit + Reformat).
    // Built once per branch rather than once per symbol: it is the same map every time round, and rebuilding it
    // inside the loop made recording one branch's states quadratic in the number of symbols in scope.
    auto new_symbol_mem_info_map = SymbolMemoryMap(new_symbol_mem_info.begin(), new_symbol_mem_info.end());

    for (auto &&[sym, old_mem_status] : pre_analysis_mem_info) {
      sym->MemInfo->FillFromSnapshot(old_mem_status);

      // Save this memory status for subsequent inter-branch
      // status comparisons.
      sym_mem_info[sym].EmplaceBack(branch, new_symbol_mem_info_map[sym]);
    }
  }

  // Add the pre-analysis memory states as a "final" branch
  // (just for comparison purposes).
  for (auto &&[sym, mem_info_list] : pre_analysis_mem_info) {
    sym_mem_info[sym].EmplaceBack(nullptr, std::move(mem_info_list));
  }

  // Get the first "non-terminating" branch, and update the
  // symbols to reflect its memory state.
  const auto non_terminating_branch = genex::find_if(
    branches, [](auto const &x) { return not x->Body->Terminates(); });
  const auto first_branch = non_terminating_branch == branches.end() ? parent : *non_terminating_branch;
  const auto first_branch_index = non_terminating_branch != branches.end()
    ? genex::iterators::distance(branches.begin(), non_terminating_branch)
    : -1;
  const auto first_branch_mem_info_getter = [&](auto const &branch_mem_info) {
    return first_branch_index != -1
      ? branch_mem_info.At(static_cast<std::size_t>(first_branch_index)).second
      : branch_mem_info.Back().second;
  };

  const auto has_else_branch = not branches.IsEmpty()
    ? branches.Back()->Patterns[0]->To<CasePatternVariantElseAst>()
    : nullptr;
  const auto skip_else = has_else_branch and has_else_branch->MarkedForIterLoopExit();

  // Check for consistency among the branches' symbols' memory
  // states.
  for (auto const &[sym, branches_memory_info_lists] : sym_mem_info) {
    auto first_branch_mem_info = first_branch_mem_info_getter(branches_memory_info_lists);

    // Assuming all new memory states are consistent across
    // branches, update to the first "new" state list.
    sym->MemInfo->FillFromSnapshot(first_branch_mem_info);

    // Check the new memory status for each symbol is
    // consistent across all branches that don't terminate.
    auto applicable_branch_memory_info_lists = branches_memory_info_lists
      | genex::views::remove_if([&](auto const &x) {
        return x.first == nullptr or x.first->Body->Terminates()
          or (skip_else and not branches.IsEmpty() and x.first == branches.Back());
      })
      | genex::to<Vec>();

    for (auto const &[branch, branch_memory_info_list] : applicable_branch_memory_info_lists) {
      // Check for consistent initialization.
      if ((spp::get<0>(first_branch_mem_info.AstInitialization) == nullptr)
        != (spp::get<0>(branch_memory_info_list.AstInitialization) == nullptr)) {
        sym->MemInfo->IsInconsistentlyInitialized = {first_branch, branch};
      }

      // Check for consistent moved state.
      if ((spp::get<0>(first_branch_mem_info.AstMoved) == nullptr)
        != (spp::get<0>(branch_memory_info_list.AstMoved) == nullptr)) {
        sym->MemInfo->IsInconsistentlyMoved = {first_branch, branch};
      }

      // Check for consistent partial moves.
      if (first_branch_mem_info.AstPartialMoves != branch_memory_info_list.AstPartialMoves) {
        sym->MemInfo->IsInconsistentlyPartiallyMoved = {first_branch, branch};
      }

      // Check for consistent escaping borrows, from both ends
      // of the link: a symbol can be the coroutine handle that
      // holds the borrows, or the owner of the memory they
      // borrow, and only the second is what a later use of that
      // memory (eg moving it) is checked against.
      if (first_branch_mem_info.AstContainedEscapingBorrows != branch_memory_info_list.AstContainedEscapingBorrows
        or EscapingBorrowContainersDiffer(
          first_branch_mem_info.AstContainersOfEscapingBorrows,
          branch_memory_info_list.AstContainersOfEscapingBorrows)) {
        sym->MemInfo->IsInconsistentlyBorrowEscaping = {first_branch, branch};
      }
    }
  }
}
