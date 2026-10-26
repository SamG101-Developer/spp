module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.array_literal_explicit_elements_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coerce;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
ArrayLiteralExplicitElementsAst::ArrayLiteralExplicitElementsAst(
  decltype(TokL) &&tok_l,
  decltype(Elems) &&elements,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Elems(std::move(elements)),
  TokR(std::move(tok_r)) {
  // Default the two tokens.
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokL, SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokR, SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

ArrayLiteralExplicitElementsAst::~ArrayLiteralExplicitElementsAst() = default;

auto ArrayLiteralExplicitElementsAst::EqualsArrayLiteralExplicitElements(
  ArrayLiteralExplicitElementsAst const &other) const -> Ordering {
  // If two explicit array asts don't have the same size, they
  // cannot be equal.
  if (Elems.size() != other.Elems.size()) { return Ordering::less; }

  // Ensure each element of the two array literals are equal.
  // The length checks prevents trailing elements.
  for (auto i = 0uz; i < Elems.size(); ++i) {
    if (*Elems[i] != *other.Elems[i]) { return Ordering::less; }
  }
  return Ordering::equal;
}

auto ArrayLiteralExplicitElementsAst::Equals(
  ExpressionAst const &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsArrayLiteralExplicitElements(*this);
}

auto ArrayLiteralExplicitElementsAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL != nullptr ? TokL->PosStart() : Elems.empty() ? 0 : Elems.front()->PosStart();
}

auto ArrayLiteralExplicitElementsAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR != nullptr ? TokR->PosEnd() : Elems.empty() ? 0 : Elems.back()->PosEnd();
}

auto ArrayLiteralExplicitElementsAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<ArrayLiteralExplicitElementsAst>(
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
}

auto ArrayLiteralExplicitElementsAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokL);
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

/// [CHECKED]
auto ArrayLiteralExplicitElementsAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Analyse the element inside the array. Also enforce that
  // the element is an acceptable primary expression, ie not
  // a non-zero-TypeAst, or a TokenAst.
  for (auto const &elem : Elems) {
    elem->Stage7_AnalyseSemantics(sm, meta);
    common_errors::RequirePrimary(*elem, *sm);
  }

  // Determine the "correct type" that all elements will be
  // compared against; a pre-defined array type could have
  // been given, allowing a variant element type to be used;
  // otherwise just use the 0th element.
  const auto z_elem = Elems[0].get();
  const auto from_target = meta->AssignmentTargetType != nullptr and
    type_predicates::IsTypeArray(TypeRef::ForKindCheck(*meta->AssignmentTargetType, *sm->CurrentScope), *sm->CurrentScope);

  const auto z_type = from_target
    ? meta->AssignmentTargetType->LastTypePart()->GnArgGroup->At("T")->TypeVal
    : z_elem->InferType(sm, meta);

  common_errors::RequireNotBorrowed(*z_type, *z_elem, "array element type", *sm);

  // Check all elements have the same type as the "correct
  // type". When the type came from the 0th element, that
  // element is skipped as it trivially matches itself;
  // when it came from the target, every element (including
  // the 0th) must be validated.
  for (auto const &c_elem : Elems | genex::views::ptr | genex::views::drop(from_target ? 0uz : 1uz)) {
    auto c_type = c_elem->InferType(sm, meta);

    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(
        TypeRef::Of(*z_type, *sm->CurrentScope), TypeRef::Of(*c_type, *sm->CurrentScope), *sm->CurrentScope,
        *sm->CurrentScope),
      ERR_ARGS(*z_elem, *z_type, *c_elem, *c_type));

    // A function named as an element stands for the overload the
    // element type asks for.
    fn_values::InstantiateFnValue(
      TypeRef::Of(*c_type, *sm->CurrentScope), TypeRef::Of(*z_type, *sm->CurrentScope), sm, meta);
  }

  // Analyse the inferred array type to generate the generic
  // implementation for future analysis; the first occurrence of
  // Vec[S32] will not exist in the symbol table, so add it now.
  _InferredType = _BuildType(sm, meta);
}

/// [CHECKED]
auto ArrayLiteralExplicitElementsAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Alias the common utils functions and types.
  IMPORT_UTILS;

  // Check the memory of each element in the array literal.
  for (auto const &elem : Elems) {
    elem->Stage8_CheckMemory(sm, meta);
    mem_utils::ValidateSymbolMemory(*elem, *elem, *sm, meta, {.MarkMoves = false});
  }
}

auto ArrayLiteralExplicitElementsAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;

  // Runtime allocation. This pathway generates each element
  // and then uses alloca into the entry block of the
  // function. Unless every element itself is constant then
  // it can use constant array to optimize away the GEPs.
  if (not ctx->InConstantContext) {
    // Collect the llvm generated versions of the elements,
    // ensuring the validity of each element (debug only). Each
    // is coerced into the array's element type, as any slot is:
    // "[n, Some(val=1)]" into "[Opt[S32]; 2]" holds two variants,
    // and "[f, g]" into "[FunRef[..]; 2]" two function values.
    const auto uid = "." + Uid();
    const auto elem_ref = InferTypeRef(sm, meta).Symbol->TypeArgRef("T");
    auto llvm_rt_elems = Vec<llvm::Value*>{};
    llvm_rt_elems.reserve(Elems.size());
    for (auto const &spp_elem : Elems) {
      auto llvm_rt_elem = spp_elem->Stage11_CodeGen(sm, meta, ctx);
      llvm_rt_elem = codegen::CoerceToSlot(
        llvm_rt_elem, elem_ref, spp_elem->InferTypeRef(sm, meta), *sm, "array.explicit.elem" + uid, ctx);
      SPP_ASSERT(llvm_rt_elem != nullptr);
      llvm_rt_elems.emplace_back(llvm_rt_elem);
    }

    // Create the array type. The array type wraps the llvm
    // element type, and the length is also provided. This
    // lowers to the llvm special array [T * n] type.
    const auto llvm_rt_elem_ty = codegen::GetLlvmTypeOf(elem_ref, ctx);
    const auto llvm_rt_arr_ty = llvm::ArrayType::get(
      llvm_rt_elem_ty, llvm_rt_elems.size());
    SPP_ASSERT(llvm_rt_arr_ty != nullptr);

    // If every element came back a constant then so is the
    // array, and it can be produced as a value rather than
    // materialised, use the equivalent to the constant path.
    const auto all_elems_constant = genex::all_of(llvm_rt_elems, [&](auto const *llvm_rt_elem) {
      return llvm::isa<llvm::Constant>(llvm_rt_elem) and llvm_rt_elem->getType() == llvm_rt_elem_ty;
    });

    if (all_elems_constant) {
      auto llvm_ct_elems = Vec<llvm::Constant*>{};
      llvm_ct_elems.reserve(llvm_rt_elems.size());
      for (auto *llvm_rt_elem : llvm_rt_elems) {
        const auto llvm_ct_elem = llvm::cast<llvm::Constant>(
          llvm_rt_elem);
        SPP_ASSERT(llvm_ct_elem != nullptr);
        llvm_ct_elems.emplace_back(llvm_ct_elem);
      }
      return llvm::ConstantArray::get(
        llvm_rt_arr_ty, llvm_ct_elems);
    }

    // Allocate the array into the enclosing function using
    // the uniform entry alloca function.
    const auto llvm_rt_arr_alloc = codegen::LlvmEntryAlloca(
      llvm_rt_arr_ty, "array.explicit.alloca" + uid, ctx);

    // Finally, place the runtime generated elements in the
    // array allocation, using the GEP and store commands.
    for (auto i = 0uz; i < llvm_rt_elems.size(); ++i) {
      const auto idx0 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, 0));
      const auto idx1 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, i));
      const auto llvm_rt_elem_ptr = ctx->Builder.CreateGEP(
        llvm_rt_arr_ty, llvm_rt_arr_alloc, {idx0, idx1});

      SPP_ASSERT(llvm_rt_elem_ptr != nullptr);
      ctx->Builder.CreateStore(llvm_rt_elems[i], llvm_rt_elem_ptr);
    }

    // Return the array by value. The "llvm_rt_arr_alloc"
    // is by pointer on the stack where the array is located.
    return ctx->Builder.CreateLoad(
      llvm_rt_arr_ty, llvm_rt_arr_alloc, "array.explicit.result" + uid);
  }

  // Comptime array creation. This pathway generated an array
  // using "constant" values (comptime-known values). Each is
  // coerced into the element type, as at runtime; a variant
  // member comes back laid out as the variant in another shape,
  // which makes the array one too ("BuildAggregate").
  {
    const auto uid = "." + Uid();
    const auto elem_ref = InferTypeRef(sm, meta).Symbol->TypeArgRef("T");
    auto llvm_ct_elems = Vec<Pair<unsigned, llvm::Value*>>{};
    llvm_ct_elems.reserve(Elems.size());
    for (auto const &[i, spp_elem] : Elems | genex::views::ptr | genex::views::enumerate) {
      const auto llvm_ct_elem = codegen::CoerceToSlot(
        spp_elem->Stage11_CodeGen(sm, meta, ctx), elem_ref, spp_elem->InferTypeRef(sm, meta), *sm,
        "array.ct.elem" + uid, ctx);
      SPP_ASSERT(llvm::isa<llvm::Constant>(llvm_ct_elem));
      llvm_ct_elems.emplace_back(static_cast<unsigned>(i), llvm_ct_elem);
    }

    // The array type wraps the element type, and the length is
    // also provided. This lowers to the llvm [T * n] type.
    const auto llvm_ct_arr_ty = llvm::ArrayType::get(codegen::GetLlvmTypeOf(elem_ref, ctx), Elems.size());
    return codegen::BuildAggregate(llvm_ct_arr_ty, llvm_ct_elems, "array.ct" + uid, ctx);
  }
}

auto ArrayLiteralExplicitElementsAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Convert the inner elements to compile-time values. One that
  // has none raises in its own evaluation (E3 for a runtime name,
  // E77 for an expression with no comp-time form). The elements
  // are only read: in a "cmp" function's body, writing a value
  // back would hand it to every later call, and to the runtime
  // code generated from the body.
  auto cmp_elems = Vec<Unique<ExpressionAst>>();
  for (auto const &elem : Elems) {
    elem->EvaluateCompTime(sm, meta);
    cmp_elems.emplace_back(std::move(meta->CompTimeResult));
  }

  // Wrap the comp-time array value, in the translated
  // asts derived from requesting comp-time evaluation. It keeps
  // the type stage 7 settled on, which is its target's element
  // type ("[Some(..), None]" is an "[Opt[T]; 2]"), not its first
  // element's.
  auto result = MakeUnique<ArrayLiteralExplicitElementsAst>(
    nullptr, std::move(cmp_elems), nullptr);
  result->_InferredType = _InferredType;
  meta->CompTimeResult = std::move(result);
}

auto ArrayLiteralExplicitElementsAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // The type stage 7 settled on; an array not analysed yet has its
  // type built as it is.
  return _InferredType != nullptr ? _InferredType : _BuildType(sm, meta);
}

auto ArrayLiteralExplicitElementsAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The array type is built as syntax ("Arr[T, n]", "_BuildType"); resolved where it is read.
  const auto type = InferType(sm, meta);
  return type != nullptr ? TypeRef::Of(*type, *sm->CurrentScope) : TypeRef();
}

auto ArrayLiteralExplicitElementsAst::_BuildType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  IMPORT_UTILS;

  // Create a "T" type and "n" size, for the array type. If
  // a pre-defined array type has been given (ie the assignment
  // target type), pull the element type from it so a variant
  // element type is preserved; otherwise use the 0th element's
  // inferred type.
  auto size_tok = MakeUnique<TokenAst>(
    TokL->PosStart(), lex::SppTokenType::LX_NUMBER,
    std::to_string(Elems.size()));
  auto size_gen = MakeUnique<IntegerLiteralAst>(
    nullptr, std::move(size_tok), "uz");

  auto elem_gen = meta->AssignmentTargetType != nullptr
    and type_predicates::IsTypeArray(TypeRef::ForKindCheck(*meta->AssignmentTargetType, *sm->CurrentScope), *sm->CurrentScope)
    ? AstCloneShared(meta->AssignmentTargetType->LastTypePart()->GnArgGroup->At("T")->TypeVal)
    : Elems[0]->InferType(sm, meta);

  // Create an array type with the inferred element type and
  // size.
  auto array_type = generate::common_types::ArrayType(PosStart(), std::move(elem_gen), std::move(size_gen));
  array_type->Stage7_AnalyseSemantics(sm, meta);
  return array_type;
}

auto ArrayLiteralExplicitElementsAst::ReadExpr(
  ExprSubst const &sub) const -> Shared<ExpressionAst> {
  // Each element is an expression, so map them all.
  auto elems = Vec<Unique<ExpressionAst>>();
  elems.reserve(Elems.size());
  for (auto const &elem : Elems) {
    elems.emplace_back(AstClone(elem->ReadExpr(sub)));
  }
  return MakeShared<ArrayLiteralExplicitElementsAst>(
    AstClone(TokL), std::move(elems), AstClone(TokR));
}

auto ArrayLiteralExplicitElementsAst::IsAllowedInDefault() const -> bool {
  // Check every element - one bad one prevents the entire
  // ast from being allowed in this specific context.
  for (auto const &x : Elems) {
    if (not x->IsAllowedInDefault()) { return false; }
  }
  return true;
}

SPP_MOD_END
