#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::TypeIdentifierAst::TypeIdentifierAst(
    const std::size_t pos,
    decltype(name) &&name,
    decltype(generic_args) &&generic_args):
    name(std::move(name)),
    generic_args(std::move(generic_args)),
    m_pos(pos) {
}


auto spp::asts::TypeIdentifierAst::pos_start() const -> std::size_t {
    return m_pos;
}


auto spp::asts::TypeIdentifierAst::pos_end() const -> std::size_t {
    return generic_args ? generic_args->pos_end() : m_pos + name.length();
}


spp::asts::TypeIdentifierAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    raw_string.append(name);
    SPP_STRING_APPEND(generic_args);
    SPP_STRING_END;
}


auto spp::asts::TypeIdentifierAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    formatted_string.append(name);
    SPP_PRINT_APPEND(generic_args);
    SPP_PRINT_END;
}


auto spp::asts::TypeIdentifierAst::from_identifier(IdentifierAst const &identifier) -> std::unique_ptr<TypeIdentifierAst> {
    return std::make_unique<TypeIdentifierAst>(identifier.pos_start(), icu::UnicodeString(identifier.val), nullptr);
}


auto spp::asts::TypeIdentifierAst::ns_parts() const -> std::vector<IdentifierAst const*> {
    return {};
}


auto spp::asts::TypeIdentifierAst::type_parts() const -> std::vector<TypeIdentifierAst const*> {
    return std::vector{this};
}


auto spp::asts::TypeIdentifierAst::without_generics() const -> std::unique_ptr<TypeAst> {
    return std::make_unique<TypeIdentifierAst>(m_pos, icu::UnicodeString(name), nullptr);
}


auto spp::asts::TypeIdentifierAst::get_convention() const -> ConventionAst* {
    return nullptr;
}


auto spp::asts::TypeIdentifierAst::substitute_generics(std::vector<GenericArgumentAst*> &&) const -> std::unique_ptr<TypeAst> {
    // todo
    return std::make_unique<TypeIdentifierAst>(m_pos, icu::UnicodeString(name), nullptr);
}


auto spp::asts::TypeIdentifierAst::contains_generic(TypeAst const *) const -> bool {
    // todo
    return false;
}


auto spp::asts::TypeIdentifierAst::set_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) -> std::unique_ptr<TypeAst> {
    return std::make_unique<TypeIdentifierAst>(m_pos, icu::UnicodeString(name), std::move(arg_group));
}


auto spp::asts::TypeIdentifierAst::with_convention() const -> std::unique_ptr<TypeAst> {
    // todo
    return std::make_unique<TypeIdentifierAst>(m_pos, icu::UnicodeString(name), nullptr);
}
