#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/utils/strings.hpp>

#include <genex/views/to.hpp>
#include <genex/views/transform.hpp>


spp::asts::IdentifierAst::IdentifierAst(
    const std::size_t pos,
    decltype(val) &&val) :
    val(std::move(val)),
    m_pos(pos) {
}


spp::asts::IdentifierAst::IdentifierAst(IdentifierAst const &other) :
    val(other.val),
    m_pos(other.m_pos) {
}


auto spp::asts::IdentifierAst::pos_start() const -> std::size_t {
    return m_pos;
}


auto spp::asts::IdentifierAst::pos_end() const -> std::size_t {
    return m_pos + val.length();
}


auto spp::asts::IdentifierAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<IdentifierAst>(m_pos, std::string(val));
}


spp::asts::IdentifierAst::operator std::string() const {
    return val;
}


auto spp::asts::IdentifierAst::print(meta::AstPrinter &) const -> std::string {
    return val;
}


auto spp::asts::IdentifierAst::equals(
    ExpressionAst const &other) const
    -> std::weak_ordering {
    return other.equals_identifier(*this);
}


auto spp::asts::IdentifierAst::equals_identifier(
    IdentifierAst const &other) const
    -> std::weak_ordering {
    if (val == other.val) {
        return std::weak_ordering::equivalent;
    }
    return std::weak_ordering::less;
}


auto spp::asts::IdentifierAst::operator<=>(IdentifierAst const &that) const -> std::weak_ordering {
    return val <=> that.val;
}


auto spp::asts::IdentifierAst::operator+(IdentifierAst const &that) const -> IdentifierAst {
    return IdentifierAst(m_pos, val + that.val);
}


auto spp::asts::IdentifierAst::operator+(std::string const &that) const -> IdentifierAst {
    return IdentifierAst(m_pos, val + that);
}


auto spp::asts::IdentifierAst::from_type(TypeAst const &val) -> std::unique_ptr<IdentifierAst> {
    return std::make_unique<IdentifierAst>(val.pos_start(), std::string(val.type_parts().back()->name));
}


auto spp::asts::IdentifierAst::to_function_identifier() const -> std::unique_ptr<IdentifierAst> {
    return std::make_unique<IdentifierAst>(m_pos, "$" + spp::utils::strings::snake_to_pascal(val));
}


auto spp::asts::IdentifierAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Check there is a symbol with the same name in the current scope.
    if (not sm->current_scope->has_var_symbol(*this) and not sm->current_scope->has_ns_symbol(*this)) {
        const auto alternatives = sm->current_scope->all_var_symbols()
            | genex::views::transform([](auto &&x) { return x->name->val; })
            | genex::views::to<std::vector>();

        const auto closest_match = spp::utils::strings::closest_match(val, alternatives);
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierUnknownError>().with_args(
            *this, "identifier", closest_match).with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::IdentifierAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Extract the symbol from the current scope, as a variable symbol.
    const auto var_sym = sm->current_scope->get_var_symbol(*this);
    return var_sym->type;
}
