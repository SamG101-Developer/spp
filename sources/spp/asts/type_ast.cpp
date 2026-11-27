module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_resolution;
import spp.asts.generic_argument_group_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeId);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::TypeAst::TypeAst() :
  _CachedWithoutGns(nullptr),
  _CachedStringification("") {
}

spp::asts::TypeAst::~TypeAst() = default;

auto spp::asts::TypeAst::ReadExpr(
  ExprSubst const &sub) const -> Shared<ExpressionAst> {
  IMPORT_UTILS;
  return type_resolution::ReadType(*this, sub);
}

auto spp::asts::TypeAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // These are always "zero_type", so return init.
  const auto init = MakeUnique<ObjectInitializerAst>(AstCloneShared(this), nullptr);
  return init->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::TypeAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // The zero type's value, with no attributes to resolve.
  const auto init = MakeUnique<ObjectInitializerAst>(AstCloneShared(this), nullptr);
  init->EvaluateCompTime(sm, meta);
}

auto spp::asts::TypeAst::IsAllowedInDefault() const -> bool {
  // A type written as a value - "None", a zero-sized marker -
  // is a value, so it may appear in a default.
  return true;
}

auto spp::asts::TypeAst::WithSourceSpanOf(
  TypeAst const &written) const -> Shared<TypeAst> {
  return WithSourceSpanOf(AstCloneShared(this), written);
}

auto spp::asts::TypeAst::WithSourceSpanAt(
  Ast const &site) const -> Shared<TypeAst> {
  return WithSourceSpanAt(AstCloneShared(this), site);
}

auto spp::asts::TypeAst::WithSourceSpanOf(
  Shared<TypeAst> &&type, TypeAst const &written) -> Shared<TypeAst> {
  // A type rewritten more than once ("Self" substituted, then
  // qualified) keeps the text first written, not a rewrite's.
  auto out = WithSourceSpanAt(std::move(type), written);
  if (out->_HasSourceSpan) {
    out->_WrittenText = written._WrittenText.empty() ? written.ToString() : written._WrittenText;
  }
  return out;
}

auto spp::asts::TypeAst::WithSourceSpanAt(
  Shared<TypeAst> &&type, Ast const &site) -> Shared<TypeAst> {
  // Only the root is stamped, so a fresh tree sharing subtrees
  // with cached names is stamped in place. A root held anywhere
  // else (a qualified name, cached on its symbol and shared by
  // every use of it) is copied first.
  auto out = std::move(type);
  if (out.use_count() != 1) { out = AstCloneShared(out.get()); }
  const auto start = site.PosStart();
  const auto end = site.PosEnd();
  if (start != 0 and end > start) {
    out->_HasSourceSpan = true;
    out->_SpanStart = start;
    out->_SpanEnd = end;
  }
  return out;
}

auto spp::asts::TypeAst::WithGns(
  Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> {
  // Clone this type and put the arguments on its right-most part.
  auto type_clone = AstClone(this);
  if (arg_group == nullptr) { arg_group = GenericArgumentGroupAst::NewEmpty(); }
  type_clone->LastTypePart()->GnArgGroup = std::move(arg_group);

  // Different arguments make a different type, so the clone keeps no written identity: the one copied from this node (a
  // cached qualified name records its symbol's) would still name the old type, and a lookup would follow it there.
  type_clone->StampTypeId(nullptr);
  type_clone->LastTypePart()->StampTypeId(nullptr);
  return type_clone;
}

SPP_MOD_END
