module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.tuple_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_predicates;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
TupleLiteralAst::TupleLiteralAst(
  decltype(TokL) &&tok_l,
  decltype(Elems) &&elements,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Elems(std::move(elements)),
  TokR(std::move(tok_r)) {
}

TupleLiteralAst::~TupleLiteralAst() = default;

auto TupleLiteralAst::EqualsTupleLiteral(
  TupleLiteralAst const &other) const -> Ordering {
  // If two tuple asts don't have the same size, they cannot be equal.
  if (Elems.Len() != other.Elems.Len()) { return Ordering::less; }

  // Ensure each element of the two array literals are equal.
  if (genex::all_of(
    genex::views::zip(Elems | genex::views::ptr, other.Elems | genex::views::ptr) | genex::to<Vec>(),
    [](auto const &pair) { return *spp::get<0>(pair) == *spp::get<1>(pair); })) {
    return Ordering::equal;
  }
  return Ordering::less;
}

auto TupleLiteralAst::Equals(
  ExpressionAst const &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsTupleLiteral(*this);
}

auto TupleLiteralAst::PosStart() const -> std::size_t {
  // Use the "(" token.
  return TokL != nullptr ? TokL->PosStart() : Elems.IsEmpty() ? 0 : Elems.Front()->PosStart();
}

auto TupleLiteralAst::PosEnd() const -> std::size_t {
  // Use the ")" token.
  return TokR != nullptr ? TokR->PosEnd() : Elems.IsEmpty() ? 0 : Elems.Back()->PosEnd();
}

auto TupleLiteralAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TupleLiteralAst>(
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
}

auto TupleLiteralAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("(");
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND_RAW(")");
  SPP_STRING_END;
}

auto TupleLiteralAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Analyse the elements in the tuple.
  for (auto const &elem : Elems) {
    elem->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppInvalidPrimaryExpressionError>(
      not expr_utils::IsPrimaryExprTypeValid(*elem, *sm),
      {sm->CurrentScope}, ERR_ARGS(*elem));
  }

  // Check all the elements are owned by the tuple, not borrowed.
  for (auto const &elem : Elems | genex::views::ptr) {
    auto elem_type = elem->InferType(sm, meta);
    RaiseIf<SppSecondClassBorrowViolationError>(
      type_predicates::IsTypeBorrowed(*elem_type, *sm),
      {sm->CurrentScope}, ERR_ARGS(*elem, *elem_type, "tuple element type"));
  }

  // Analyse the inferred tuple type to generate the generic implementation.
  _InferredType = _BuildType(sm, meta);
}

auto TupleLiteralAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Check the memory of each element in the tuple literal.
  for (auto const &elem : Elems) {
    elem->Stage8_CheckMemory(sm, meta);
    mem_utils::ValidateSymbolMemory(*elem, *elem, *sm, meta, {.MarkMoves = false});
  }
}

auto TupleLiteralAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Convert the inner elements to compile-time values.
  auto cmp_elems = Vec<Unique<ExpressionAst>>();
  for (auto [i, elem] : Elems | genex::views::ptr | genex::views::enumerate) {
    elem->Stage9_CompTimeResolve(sm, meta);
    Elems[i] = AstClone(meta->CompTimeResult);
    cmp_elems.EmplaceBack(std::move(meta->CompTimeResult));
  }

  // Wrap the compile-time array value.
  meta->CompTimeResult = MakeUnique<TupleLiteralAst>(
    nullptr, std::move(cmp_elems), nullptr);
}

auto TupleLiteralAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  // The tuple lowers to a struct of its element types, kept in declaration order, so element "i" is field "i".
  const auto uid = "." + Uid();
  const auto tuple_type_sym = InferTypeRef(sm, meta).Symbol;
  const auto llvm_type = codegen::GetLlvmType(*tuple_type_sym, ctx);
  SPP_ASSERT(llvm_type != nullptr);

  // Runtime pathway: build the tuple in a stack slot, and load it back out to give the expression its value.
  if (not ctx->InConstantContext) {
    const auto llvm_struct_type = llvm::cast<llvm::StructType>(llvm_type);

    // Every element is generated up front, because whether the tuple as a whole is constant cannot be known until
    // they have been.
    auto elem_values = Vec<llvm::Value*>();
    elem_values.Reserve(Elems.Len());
    for (auto const &elem : Elems) {
      const auto elem_value = elem->Stage11_CodeGen(sm, meta, ctx);
      SPP_ASSERT(elem_value != nullptr);
      elem_values.EmplaceBack(elem_value);
    }

    // If they all came back constant then so is the tuple, and it can be produced as a value rather than
    // materialised: no stack slot, no per-field GEP and store, and no load to read it back. Being outside a constant
    // context only says this was not written as a "cmp" initializer, which is no statement about the elements.
    auto all_elems_constant = true;
    for (auto i = 0uz; i < elem_values.Len(); ++i) {
      const auto field_type = llvm_struct_type->getElementType(static_cast<unsigned>(i));
      if (llvm::isa<llvm::Constant>(elem_values[i]) and elem_values[i]->getType() == field_type) { continue; }
      all_elems_constant = false;
      break;
    }

    if (all_elems_constant) {
      auto llvm_ct_elems = Vec<llvm::Constant*>();
      llvm_ct_elems.Reserve(elem_values.Len());
      for (auto *elem_value : elem_values) {
        llvm_ct_elems.EmplaceBack(llvm::cast<llvm::Constant>(elem_value));
      }
      return llvm::ConstantStruct::get(llvm_struct_type, llvm_ct_elems.ToStdVector());
    }

    const auto alloca = codegen::LlvmEntryAlloca(llvm_type, "tuple.alloca" + uid, ctx);
    SPP_ASSERT(alloca != nullptr);

    // Store each element into the tuple alloca.
    for (auto i = 0uz; i < elem_values.Len(); ++i) {
      const auto elem_ptr = ctx->Builder.CreateStructGEP(
        llvm_type, alloca, static_cast<std::uint32_t>(i), "tuple.elem.ptr" + uid);
      ctx->Builder.CreateStore(elem_values[i], elem_ptr);
    }

    // Load the tuple value from the alloca and return it.
    const auto tuple_value = ctx->Builder.CreateLoad(llvm_type, alloca, "tuple.val" + uid);
    return tuple_value;
  }

  // Constant pathway: the struct constant is built from the elements' own constants.
  auto comp_elems = Vec<llvm::Constant*>();
  comp_elems.Reserve(Elems.Len());
  for (auto const &elem : Elems) {
    const auto comp_elem = elem->Stage11_CodeGen(sm, meta, ctx);
    SPP_ASSERT(comp_elem != nullptr);
    comp_elems.EmplaceBack(llvm::cast<llvm::Constant>(comp_elem));
  }
  return llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(llvm_type), comp_elems.ToStdVector());
}

auto TupleLiteralAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // The type stage 7 settled on; a tuple not analysed yet has its
  // type built as it is.
  return _InferredType != nullptr ? _InferredType : _BuildType(sm, meta);
}

auto TupleLiteralAst::_BuildType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  return TypeOfElements(Elems | genex::views::ptr | genex::to<Vec>(), PosStart(), sm, meta);
}

auto TupleLiteralAst::TypeOfElements(
  Vec<ExpressionAst*> const &elems, const std::size_t pos, ScopeManager *sm, CompilerMetaData *meta)
  -> Shared<TypeAst> {
  //
  using generate::common_types::TupleType;

  // Create a "..Ts" type, for the tuple type. A bound generic keeps
  // its parameter's name ("T"), so each element is taken as what it
  // is bound to - or an instantiation's "(T(), U())" is the generic
  // tuple "(T, U)", which has no layout to generate. Todo: TIDY
  auto types_gen = elems
    | genex::views::transform([sm, meta](auto *elem) {
      auto type = elem->InferType(sm, meta);
      const auto sym = sm->CurrentScope->FindTypeSymbol(type->WithoutConvention().get());
      if (sym != nullptr and sym->IsGn() and sym->AsBound() != sym) {
        type = sym->AsBound()->FqName()->WithConvention(AstClone(type->GetConvention()));
      }
      return type;
    })
    | genex::to<Vec>();

  // Create a tuple type with the inferred element types.
  auto tuple_type = TupleType(pos, std::move(types_gen));
  tuple_type->Stage7_AnalyseSemantics(sm, meta);
  return tuple_type;
}

auto TupleLiteralAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Shared<ExpressionAst> {
  // Each element is an expression so substitute them
  // all too.
  // An element naming a comp pack ("ns" in the pack "(0_uz, ns)" a variadic argument list is collected into) spreads
  // into the elements of the tuple it is bound to, as a type pack does in "Tup[S32, Ts]".
  auto elems = Vec<Unique<ExpressionAst>>();
  elems.Reserve(Elems.Len());
  for (auto const &elem : Elems) {
    auto sub_elem = elem->ReadExpr(sub);
    auto const *const id = elem->To<IdentifierAst>();
    auto const *const pack = id != nullptr ? sub.Written->FindVarSymbol(id) : nullptr;
    if (pack != nullptr and pack->IsVariadic and sub_elem->To<TupleLiteralAst>() != nullptr) {
      for (auto const *inner : analyse::utils::packs::CompPackElements(*sub_elem)) { elems.EmplaceBack(AstClone(inner)); }
      continue;
    }
    elems.EmplaceBack(AstClone(sub_elem));
  }
  return MakeShared<TupleLiteralAst>(AstClone(TokL), std::move(elems), AstClone(TokR));
}

auto TupleLiteralAst::IsAllowedInDefault() const -> bool {
  // Check every element - one bad one prevents the entire
  // ast from being allowed in this specific context.
  for (auto const &x : Elems) {
    if (not x->IsAllowedInDefault()) { return false; }
  }
  return true;
}

SPP_MOD_END
