module;
#include <spp/analyse/macros.hpp>
#include <spp/macros.hpp>

module spp.asts.array_literal_repeated_element_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.lex.tokens;
import spp.utils.uid;
import llvm;

SPP_MOD_BEGIN
spp::asts::ArrayLiteralRepeatedElementAst::ArrayLiteralRepeatedElementAst(
  decltype(TokL) &&tok_l,
  decltype(Elem) &&elem,
  decltype(TokSemicolon) &&tok_semicolon,
  decltype(Size) &&size,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Elem(std::move(elem)),
  TokSemicolon(std::move(tok_semicolon)),
  Size(std::move(size)),
  TokR(std::move(tok_r)) {
  // Default the three tokens.
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokSemicolon, lex::SppTokenType::TK_SEMICOLON, ";");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::ArrayLiteralRepeatedElementAst::~ArrayLiteralRepeatedElementAst() = default;

auto spp::asts::ArrayLiteralRepeatedElementAst::EqualsArrayLiteralRepeatedElement(
  ArrayLiteralRepeatedElementAst const &other) const
  -> Ordering {
  // Equality based off the element and size.
  return *Elem == *other.Elem and *Size == *other.Size ? Ordering::equal : Ordering::less;
}

auto spp::asts::ArrayLiteralRepeatedElementAst::Equals(
  ExpressionAst const &other) const
  -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsArrayLiteralRepeatedElement(*this);
}

auto spp::asts::ArrayLiteralRepeatedElementAst::PosStart() const
  -> std::size_t {
  // Use the "[" token.
  return TokL != nullptr ? TokL->PosStart() : Elem->PosStart();
}

auto spp::asts::ArrayLiteralRepeatedElementAst::PosEnd() const
  -> std::size_t {
  // Use the "]" token.
  return TokR != nullptr ? TokR->PosEnd() : Size->PosEnd();
}

auto spp::asts::ArrayLiteralRepeatedElementAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<ArrayLiteralRepeatedElementAst>(
    AstClone(TokL),
    AstClone(Elem),
    AstClone(TokSemicolon),
    AstClone(Size),
    AstClone(TokR));
}

auto spp::asts::ArrayLiteralRepeatedElementAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokL);
  SPP_STRING_APPEND(Elem);
  SPP_STRING_APPEND(TokSemicolon).append(" ");
  SPP_STRING_APPEND(Size);
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

auto spp::asts::ArrayLiteralRepeatedElementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Alias the common utils functions and types.
  using analyse::errors::SppCompileTimeConstantError;
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::errors::SppNonCopyableTypeError;
  using analyse::errors::SppSecondClassBorrowViolationError;
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
  using analyse::utils::type_utils::IsTypeBorrowed;

  // Analyse the repeated element.
  Elem->Stage7_AnalyseSemantics(sm, meta);
  Size->Stage7_AnalyseSemantics(sm, meta);

  RaiseIf<SppInvalidPrimaryExpressionError>(
    not IsPrimaryExprTypeValid(*Elem, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Elem));
  const auto elem_type = Elem->InferType(sm, meta);
  const auto elem_type_sym = sm->CurrentScope->GetTypeSymbol(elem_type.get());

  // Ensure the element type is copyable, so that is can be repeated in the array.
  RaiseIf<SppNonCopyableTypeError>(
    not elem_type_sym->IsCopyable(),
    {sm->CurrentScope}, ERR_ARGS(*this, *Elem, *elem_type));

  // Check the size argument is a valid AST.
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not IsPrimaryExprTypeValid(*Size, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Size));

  // Ensure the element's type is not a borrow type, as array elements cannot be borrows.
  RaiseIf<SppSecondClassBorrowViolationError>(
    IsTypeBorrowed(*elem_type, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Elem, *elem_type, "repeated array element type"));

  // Ensure the size is a constant expression (if symbolic).
  auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope);
  Size->Stage9_CompTimeResolve(&tm, meta);

  RaiseIf<SppCompileTimeConstantError>(
    meta->CmpResult == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*Size));
  Size = AstClone(meta->CmpResult);

  // Make sure the generic array type is analysed for generic generation.
  InferType(sm, meta)->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::ArrayLiteralRepeatedElementAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Alias the common utils functions and types.
  using analyse::utils::mem_utils::ValidateSymbolMemory;

  // Check the memory of the repeated element (is it initialized etc).
  Elem->Stage8_CheckMemory(sm, meta);
  ValidateSymbolMemory(*Elem, *TokSemicolon, *sm, true, true, true, false, meta);
}

auto spp::asts::ArrayLiteralRepeatedElementAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Convert the inner element to a compile-time value.
  Elem->Stage9_CompTimeResolve(sm, meta);
  Elem = AstClone(meta->CmpResult);

  // Wrap the compile-time array value.
  meta->CmpResult = MakeUnique<ArrayLiteralRepeatedElementAst>(
    nullptr, std::move(meta->CmpResult), nullptr, AstClone(Size), nullptr);
}

auto spp::asts::ArrayLiteralRepeatedElementAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  //
  using spp::utils::Uid;

  // Get the length that the array will be created for, from the
  // generic comp arg (always resolved by now).
  const auto n = std::stoull(
    Size->To<IntegerLiteralAst>()->Val->TokenData);

  // Runtime allocation. This pathway generates the given element once,
  // and then copies the resulting value into each of the n array slots.
  if (not ctx->InConstantContext) {
    // Generate the element a single time; the resulting value is reused
    // (copied) for every slot in the array below.
    const auto llvm_rt_elem = Elem->Stage11_CodeGen(sm, meta, ctx);
    SPP_ASSERT(llvm_rt_elem != nullptr);

    // Create the array type. The array type wraps the llvm determined
    // element type, and the length is also provided. This lowers to
    // the llvm special array [T * n] type.
    const auto llvm_rt_elem_ty = llvm_rt_elem->getType();
    const auto llvm_rt_arr_ty = llvm::ArrayType::get(llvm_rt_elem_ty, n);
    SPP_ASSERT(llvm_rt_arr_ty != nullptr);

    // Allocate the array into the enclosing function using the uniform
    // entry alloca function.
    const auto uid = "." + Uid(this);
    const auto llvm_rt_arr_alloc = codegen::LlvmEntryAlloca(
      llvm_rt_arr_ty, "array.explicit.alloca" + uid, ctx);

    // Finally, copy the single generated element into every slot of the
    // array allocation, using the GEP and store commands.
    for (auto i = 0uz; i < n; ++i) {
      const auto idx0 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, 0));
      const auto idx1 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, i));
      const auto llvm_rt_elem_ptr = ctx->Builder.CreateGEP(
        llvm_rt_arr_ty, llvm_rt_arr_alloc, {idx0, idx1});

      SPP_ASSERT(llvm_rt_elem_ptr != nullptr);
      ctx->Builder.CreateStore(llvm_rt_elem, llvm_rt_elem_ptr);
    }
  }

  // Comptime array creation. This pathway generates the element once as a
  // "constant" value (comptime-known), then copies it into every slot.
  {
    // Generate the element a single time, ensuring its validity after a
    // constant cast (debug only). The resulting constant is reused for
    // every slot in the array below.
    const auto llvm_ct_elem = llvm::cast<llvm::Constant>(
      Elem->Stage11_CodeGen(sm, meta, ctx));
    SPP_ASSERT(llvm_ct_elem != nullptr);

    // Create the array type. The array type wraps the llvm determined
    // element type, and the length is also provided. This lowers to
    // the llvm special array [T * n] type.
    const auto llvm_ct_elem_ty = llvm_ct_elem->getType();
    const auto llvm_ct_arr_ty = llvm::ArrayType::get(llvm_ct_elem_ty, n);
    SPP_ASSERT(llvm_ct_arr_ty != nullptr);

    // Allocate the array into the enclosing function using the llvm
    // constant array creation, reusing the same constant for every slot.
    auto llvm_ct_elems = Vec<llvm::Constant*>{};
    llvm_ct_elems.Reserve(n);
    for (auto i = 0uz; i < n; ++i) { llvm_ct_elems.EmplaceBack(llvm_ct_elem); }

    const auto arr_alloc = llvm::ConstantArray::get(
      llvm_ct_arr_ty, llvm_ct_elems.ToStdVector());
    return arr_alloc;
  }
}

auto spp::asts::ArrayLiteralRepeatedElementAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Alias the common utils functions and types.
  using generate::common_types::ArrayType;

  // Create the standard "std::array::Arr[T, n]" type, with generic arguments.
  auto elem_type = Elem->InferType(sm, meta);
  auto array_type = ArrayType(
    TokL->PosStart(), std::move(elem_type), AstClone(Size));
  array_type->Stage7_AnalyseSemantics(sm, meta);
  return array_type;
}

SPP_MOD_END
