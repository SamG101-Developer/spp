#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/utils/strings.hpp>


spp::asts::IdentifierAst::IdentifierAst(
    const std::size_t pos,
    decltype(val) &&val):
    val(std::move(val)),
    m_pos(pos) {
}


auto spp::asts::IdentifierAst::pos_start() const -> std::size_t {
    return m_pos;
}


auto spp::asts::IdentifierAst::pos_end() const -> std::size_t {
    return m_pos + val.length();
}


spp::asts::IdentifierAst::operator icu::UnicodeString() const {
    return val;
}


auto spp::asts::IdentifierAst::print(meta::AstPrinter &) const -> icu::UnicodeString {
    return val;
}


auto spp::asts::IdentifierAst::operator==(IdentifierAst const &that) const -> bool {
    return val == that.val;
}


auto spp::asts::IdentifierAst::operator+(IdentifierAst const &that) const -> IdentifierAst {
    return IdentifierAst(m_pos, val + that.val);
}


auto spp::asts::IdentifierAst::operator+(icu::UnicodeString const &that) const -> IdentifierAst {
    return IdentifierAst(m_pos, val + that);
}


auto spp::asts::IdentifierAst::from_type(TypeAst const &val) -> std::unique_ptr<IdentifierAst> {
    return std::make_unique<IdentifierAst>(val.pos_start(), icu::UnicodeString(val.type_parts().back()->name));
}


auto spp::asts::IdentifierAst::to_function_identifier() const -> std::unique_ptr<IdentifierAst> {
    return std::make_unique<IdentifierAst>(m_pos, "$" + utils::strings::snake_to_pascal(val));
}
