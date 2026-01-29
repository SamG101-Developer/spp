module;
#include <spp/macros.hpp>

module spp.asts.fold_expression_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;


spp::asts::FoldExpressionAst::FoldExpressionAst(
    decltype(tok_ellipsis) &&tok_ellipsis) :
    tok_ellipsis(std::move(tok_ellipsis)) {
}


spp::asts::FoldExpressionAst::~FoldExpressionAst() = default;


auto spp::asts::FoldExpressionAst::pos_start() const
    -> std::size_t {
    return tok_ellipsis->pos_start();
}


auto spp::asts::FoldExpressionAst::pos_end() const
    -> std::size_t {
    return tok_ellipsis->pos_end();
}


auto spp::asts::FoldExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<FoldExpressionAst>(
        ast_clone(tok_ellipsis));
}


spp::asts::FoldExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_END;
}


auto spp::asts::FoldExpressionAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    return generate::common_types::void_type(pos_start());
}
