module spp.asts.boolean_literal_ast;
import spp.asts.token_ast;


spp::asts::BooleanLiteralAst::BooleanLiteralAst(
    decltype(tok_bool) &&tok_bool) :
    tok_bool(std::move(tok_bool)) {
}


spp::asts::BooleanLiteralAst::~BooleanLiteralAst() = default;


auto spp::asts::BooleanLiteralAst::to_rust() const
    -> std::string {
    return tok_bool->to_rust();
}
