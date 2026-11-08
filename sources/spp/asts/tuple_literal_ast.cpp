module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.tuple_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.convention_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.spread_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts, ExpressionAst);
use_alias(spp::asts, SpreadExpressionAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
namespace {
  /// The value an element gives: a spread's tuple, or the element itself.
  auto ValueOf(ExpressionAst &elem) -> ExpressionAst& {
    auto spread = elem.To<SpreadExpressionAst>();
    return spread != nullptr ? *spread->Expr : elem;
  }
}

spp::asts::TupleLiteralAst::TupleLiteralAst(
  decltype(TokL) &&tok_l,
  decltype(Elems) &&elements,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Elems(std::move(elements)),
  TokR(std::move(tok_r)) {
}

spp::asts::TupleLiteralAst::~TupleLiteralAst() = default;

auto spp::asts::TupleLiteralAst::EqualsTupleLiteral(
  TupleLiteralAst const &other) const -> Ordering {
  // If two tuple asts don't have the same size, they cannot be equal.
  if (Elems.size() != other.Elems.size()) { return Ordering::less; }

  // Ensure each element of the two array literals are equal.
  if (genex::all_of(
    genex::views::zip(Elems | genex::views::ptr, other.Elems | genex::views::ptr) | genex::to<Vec>(),
    [](auto const &pair) { return *spp::get<0>(pair) == *spp::get<1>(pair); })) {
    return Ordering::equal;
  }
  return Ordering::less;
}

auto spp::asts::TupleLiteralAst::Equals(
  ExpressionAst const &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsTupleLiteral(*this);
}

auto spp::asts::TupleLiteralAst::PosStart() const -> std::size_t {
  // Use the "(" token.
  return TokL != nullptr ? TokL->PosStart() : Elems.empty() ? 0 : Elems.front()->PosStart();
}

auto spp::asts::TupleLiteralAst::PosEnd() const -> std::size_t {
  // Use the ")" token.
  return TokR != nullptr ? TokR->PosEnd() : Elems.empty() ? 0 : Elems.back()->PosEnd();
}

auto spp::asts::TupleLiteralAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, and the type stage 7 settled
  // on, as an array literal's clone does.
  auto ast = MakeUnique<TupleLiteralAst>(
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
  ast->_InferredType = _InferredType;
  ast->_SpreadTemps = AstCloneVec(_SpreadTemps);
  return ast;
}

auto spp::asts::TupleLiteralAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("(");
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND_RAW(")");
  SPP_STRING_END;
}

auto spp::asts::TupleLiteralAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Expand each spread of a closed tuple into its elements ("(a, ..t)" is
  // "(a, t.0, t.1)"). An open one's (a template's pack) stays, giving the type
  // its elements ("ElementTypes"); each instance expands it.
  auto elems = Vec<Unique<ExpressionAst>>();
  elems.reserve(Elems.size());
  for (auto &elem : Elems) {
    const auto spr = elem->To<SpreadExpressionAst>();
    auto exp = spr != nullptr
      ? expr_utils::ExpandTupleSpread(*spr->Expr, *spr->TokUnpack, sm, meta)
      : Nullopt;

    // Todo: Document this better.
    if (not exp.has_value()) { // Open
      // A borrowed pack's elements are borrows, and a tuple holds none: decided
      // off the pack in a template.
      const auto pack_name = spr != nullptr ? spr->Expr->To<IdentifierAst>() : nullptr;
      const auto pack = pack_name != nullptr ? sm->CurrentScope->FindVarSymbol(pack_name) : nullptr;
      RaiseIf<SppSecondClassBorrowViolationError>(
        pack != nullptr and pack->Pack.has_value()
        and pack->Pack->Element->GetConvention() != nullptr,
        ERR_ARGS(*spr, *pack->Pack->Element, "tuple element type"));

      elems.emplace_back(std::move(elem));
      continue;
    }
    if (exp->Temp != nullptr) { _SpreadTemps.emplace_back(std::move(exp->Temp)); }
    for (auto &part : exp->Elems) { elems.emplace_back(std::move(part)); }
  }
  Elems = std::move(elems);

  // Analyse the elements in the tuple.
  for (auto const &elem : Elems) {
    elem->Stage7_AnalyseSemantics(sm, meta);
    common_errors::RequirePrimary(ValueOf(*elem), *sm);
  }

  // Check all the elements are owned by the tuple, not borrowed.
  for (auto const &elem : Elems | genex::views::ptr) {
    auto elem_type = elem->InferType(sm, meta);
    common_errors::RequireNotBorrowed(*elem_type, *elem, "tuple element type", *sm);
  }

  // Analyse the inferred tuple type to generate the generic implementation.
  _InferredType = _BuildType(sm, meta);
}

auto spp::asts::TupleLiteralAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Check the memory of each element in the tuple literal, after the temporaries spread elements are read off.
  for (auto const &temp : _SpreadTemps) { temp->Stage8_CheckMemory(sm, meta); }
  for (auto const &elem : Elems) {
    elem->Stage8_CheckMemory(sm, meta);
    mem_utils::ValidateSymbolMemory(*elem, *elem, *sm, meta, {.MarkMoves = false});
  }
}

auto spp::asts::TupleLiteralAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  for (auto const &temp : _SpreadTemps) { temp->Stage11_CodeGen(sm, meta, ctx); }

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
    elem_values.reserve(Elems.size());
    for (auto const &elem : Elems) {
      const auto elem_value = elem->Stage11_CodeGen(sm, meta, ctx);
      SPP_ASSERT(elem_value != nullptr);
      elem_values.emplace_back(elem_value);
    }

    // If they all came back constant then so is the tuple, and it can be produced as a value rather than
    // materialised: no stack slot, no per-field GEP and store, and no load to read it back. Being outside a constant
    // context only says this was not written as a "cmp" initializer, which is no statement about the elements.
    auto all_elems_constant = true;
    for (auto i = 0uz; i < elem_values.size(); ++i) {
      const auto field_type = llvm_struct_type->getElementType(static_cast<unsigned>(i));
      if (llvm::isa<llvm::Constant>(elem_values[i]) and elem_values[i]->getType() == field_type) { continue; }
      all_elems_constant = false;
      break;
    }

    if (all_elems_constant) {
      auto llvm_ct_elems = Vec<llvm::Constant*>();
      llvm_ct_elems.reserve(elem_values.size());
      for (auto *elem_value : elem_values) {
        llvm_ct_elems.emplace_back(llvm::cast<llvm::Constant>(elem_value));
      }
      return llvm::ConstantStruct::get(llvm_struct_type, llvm_ct_elems);
    }

    const auto alloca = codegen::LlvmEntryAlloca(llvm_type, "tuple.alloca" + uid, ctx);
    SPP_ASSERT(alloca != nullptr);

    // Store each element into the tuple alloca.
    for (auto i = 0uz; i < elem_values.size(); ++i) {
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
  comp_elems.reserve(Elems.size());
  for (auto const &elem : Elems) {
    const auto comp_elem = elem->Stage11_CodeGen(sm, meta, ctx);
    SPP_ASSERT(comp_elem != nullptr);
    comp_elems.emplace_back(llvm::cast<llvm::Constant>(comp_elem));
  }
  return llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(llvm_type), comp_elems);
}

auto spp::asts::TupleLiteralAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Convert the inner elements to compile-time values. They are
  // only read, as an array literal's are.
  auto cmp_elems = Vec<Unique<ExpressionAst>>();
  for (auto const &elem : Elems) {
    elem->EvaluateCompTime(sm, meta);
    cmp_elems.emplace_back(std::move(meta->CompTimeResult));
  }

  // Wrap the compile-time tuple value. It keeps the type stage 7
  // settled on ("(None, 1_u64)" as an "(Opt[U64], U64)"), as an
  // array literal's value does, not its elements' own.
  auto result = MakeUnique<TupleLiteralAst>(nullptr, std::move(cmp_elems), nullptr);
  result->_InferredType = _InferredType;
  meta->CompTimeResult = std::move(result);
}

auto spp::asts::TupleLiteralAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // The type stage 7 settled on; a tuple not analysed yet has its
  // type built as it is.
  return _InferredType != nullptr ? _InferredType : _BuildType(sm, meta);
}

auto spp::asts::TupleLiteralAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The tuple type is built as syntax ("Tup[...]", "_BuildType");
  // resolved where it is read.
  const auto type = InferType(sm, meta);
  return type != nullptr ? TypeRef::Of(*type, *sm->CurrentScope) : TypeRef();
}

auto spp::asts::TupleLiteralAst::_BuildType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // The tuple of the elements' types, analysed.
  auto tuple_type = generate::common_types::TupleType(
    PosStart(), ElementTypes(Elems | genex::views::ptr | genex::to<Vec>(), sm, meta));
  tuple_type->Stage7_AnalyseSemantics(sm, meta);
  return tuple_type;
}

auto spp::asts::TupleLiteralAst::ElementTypes(
  Vec<ExpressionAst*> const &elems, ScopeManager *sm, CompilerMetaData *meta,
  const bool strip_conventions) -> Vec<Shared<TypeAst>> {
  // Iterate each element in the tuple, inferring its type and applying pack
  // spreading logic.
  auto types_gen = Vec<Shared<TypeAst>>();
  for (const auto elem : elems) {
    // Infer the type of each element. If we are stripping the conventions, then
    // remove them before any more processing.
    auto type = elem->InferType(sm, meta);
    if (strip_conventions) { type = AstCloneShared(type->WithoutConvention()); }

    // For a spread expression that is being moved from into this tuple, add the
    // types of each part of that pack into the type list and continue looping.
    if (elem->To<SpreadExpressionAst>() != nullptr and type->GetConvention() == nullptr) {
      for (const auto arg : type->LastTypePart()->GnArgGroup->GetTypeArgs()) {
        types_gen.emplace_back(arg->TypeVal->WithSourceSpanAt(*elem));
      }
      continue;
    }

    // Otherwise, get the type's symbol, and qualify it, and set it into the
    // type list.
    const auto sym = sm->CurrentScope->FindTypeSymbol(
      type->WithoutConvention().get());
    if (sym != nullptr and sym->IsGn() and sym->AsBound() != sym) {
      type = sym->AsBound()->FqName()->WithConvention(
        AstClone(type->GetConvention()));
    }
    types_gen.emplace_back(type->WithSourceSpanAt(*elem));
  }
  return types_gen;
}

auto spp::asts::TupleLiteralAst::ReadExpr(
  ExprSubst const &sub) const -> Shared<ExpressionAst> {
  IMPORT_UTILS;
  // Each element is an expression so substitute them all too. An element naming
  // a comp pack ("ns" in the pack "(0_uz, ns)" a variadic argument list is
  // collected into) spreads into the elements of the tuple it is bound to, as a
  // type pack does in "Tup[S32, Ts]".
  auto elems = Vec<Unique<ExpressionAst>>();
  elems.reserve(Elems.size());
  for (auto const &elem : Elems) {
    auto sub_elem = elem->ReadExpr(sub);

    // A spread of a comp pack, read as the tuple literal it is bound to, is its
    // elements.
    if (const auto spread = sub_elem->To<SpreadExpressionAst>(); spread != nullptr) {
      if (const auto tuple = spread->Expr->To<TupleLiteralAst>(); tuple != nullptr) {
        for (auto const &inner : tuple->Elems) { elems.emplace_back(AstClone(inner)); }
        continue;
      }
    }
    const auto id = elem->To<IdentifierAst>();
    const auto pack = id != nullptr ? sub.Written->FindVarSymbol(id) : nullptr;
    if (pack != nullptr and pack->IsVariadic and sub_elem->To<TupleLiteralAst>() != nullptr) {
      for (auto const *inner : packs::CompPackElements(*sub_elem)) {
        elems.emplace_back(AstClone(inner));
      }
      continue;
    }
    elems.emplace_back(AstClone(sub_elem));
  }
  return MakeShared<TupleLiteralAst>(AstClone(TokL), std::move(elems), AstClone(TokR));
}

auto spp::asts::TupleLiteralAst::IsAllowedInDefault() const -> bool {
  // Check every element - one bad one prevents the entire
  // ast from being allowed in this specific context.
  for (auto const &x : Elems) {
    if (not x->IsAllowedInDefault()) { return false; }
  }
  return true;
}

SPP_MOD_END
