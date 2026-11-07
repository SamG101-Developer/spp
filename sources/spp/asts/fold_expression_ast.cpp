module;
#include <spp/macros.hpp>

module spp.asts.fold_expression_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::FoldExpressionAst::FoldExpressionAst(
  decltype(TokEllipsis) &&tok_ellipsis) :
  TokEllipsis(std::move(tok_ellipsis)) {
}

spp::asts::FoldExpressionAst::~FoldExpressionAst() = default;

auto spp::asts::FoldExpressionAst::PosStart() const -> std::size_t {
  // Use the ".." token.
  return TokEllipsis->PosStart();
}

auto spp::asts::FoldExpressionAst::PosEnd() const -> std::size_t {
  // Use the ".." token.
  return TokEllipsis->PosEnd();
}

auto spp::asts::FoldExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<FoldExpressionAst>(
    AstClone(TokEllipsis));
}

auto spp::asts::FoldExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokEllipsis);
  SPP_STRING_END;
}

auto spp::asts::FoldExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  return TypeRef::Of(*generate::common_types_precompiled::VOID, *sm->CurrentScope);
}

SPP_MOD_END
