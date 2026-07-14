module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.fold_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::FoldExpressionAst::FoldExpressionAst(
    decltype(TokEllipsis) &&tok_ellipsis) :
    TokEllipsis(std::move(tok_ellipsis)) {
}

spp::asts::FoldExpressionAst::~FoldExpressionAst() = default;

auto spp::asts::FoldExpressionAst::PosStart() const
    -> std::size_t {
    // Use the ".." token.
    return TokEllipsis->PosStart();
}

auto spp::asts::FoldExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the ".." token.
    return TokEllipsis->PosEnd();
}

auto spp::asts::FoldExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<FoldExpressionAst>(
        AstClone(TokEllipsis));
}

auto spp::asts::FoldExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokEllipsis);
    SPP_STRING_END;
}

auto spp::asts::FoldExpressionAst::InferType(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Fold expressions are always "Void".
    using generate::common_types::VoidType;
    auto inferred = VoidType(PosStart());
    CACHE_TYPE_INFERENCE_AND_RETURN(inferred);
}

SPP_MOD_END
