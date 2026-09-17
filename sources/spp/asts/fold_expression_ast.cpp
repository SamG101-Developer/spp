module;
#include <spp/macros.hpp>

module spp.asts.fold_expression_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
FoldExpressionAst::FoldExpressionAst(
  decltype(TokEllipsis) &&tok_ellipsis) :
  TokEllipsis(std::move(tok_ellipsis)) {
}

FoldExpressionAst::~FoldExpressionAst() = default;

auto FoldExpressionAst::PosStart() const -> std::size_t {
  // Use the ".." token.
  return TokEllipsis->PosStart();
}

auto FoldExpressionAst::PosEnd() const -> std::size_t {
  // Use the ".." token.
  return TokEllipsis->PosEnd();
}

auto FoldExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<FoldExpressionAst>(
    AstClone(TokEllipsis));
}

auto FoldExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokEllipsis);
  SPP_STRING_END;
}

auto FoldExpressionAst::InferType(
  ScopeManager *, CompilerMetaData *) -> Shared<TypeAst> {
  // Fold expressions are always "Void".
  using generate::common_types::VoidType;
  return VoidType(PosStart());
}

auto FoldExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  return TypeRef::Of(*generate::common_types_precompiled::VOID, *sm->CurrentScope);
}

SPP_MOD_END
