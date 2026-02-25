module spp.asts.char_literal_ast;
import spp.asts.token_ast;


spp::asts::CharLiteralAst::CharLiteralAst(
    decltype(val) &&val) :
    LiteralAst(),
    val(std::move(val)) {
}


spp::asts::CharLiteralAst::~CharLiteralAst() = default;


auto spp::asts::CharLiteralAst::to_rust() const
    -> std::string {
    return std::string("'").append(val->to_rust()).append("'");
}
