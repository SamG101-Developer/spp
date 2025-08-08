#include <spp/asts/fold_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::FoldExpressionAst::FoldExpressionAst(decltype(tok_ellipsis) &&tok_ellipsis) :
    PrimaryExpressionAst(),
    tok_ellipsis(std::move(tok_ellipsis)) {
}


spp::asts::FoldExpressionAst::~FoldExpressionAst() = default;


auto spp::asts::FoldExpressionAst::pos_start() const -> std::size_t {
    return tok_ellipsis->pos_start();
}


auto spp::asts::FoldExpressionAst::pos_end() const -> std::size_t {
    return tok_ellipsis->pos_end();
}


spp::asts::FoldExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_END;
}


auto spp::asts::FoldExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_END;
}
