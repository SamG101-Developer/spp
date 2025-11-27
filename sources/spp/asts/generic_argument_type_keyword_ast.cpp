module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_type_keyword_ast;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.lex.tokens;


spp::asts::GenericArgumentTypeKeywordAst::GenericArgumentTypeKeywordAst(
    decltype(name) name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) val) :
    GenericArgumentTypeAst(std::move(val), mixins::OrderableTag::KEYWORD_ARG),
    name(std::move(name)),
    tok_assign(std::move(tok_assign)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


spp::asts::GenericArgumentTypeKeywordAst::~GenericArgumentTypeKeywordAst() = default;


auto spp::asts::GenericArgumentTypeKeywordAst::equals(
    GenericArgumentAst const &other) const
    -> std::strong_ordering {
    return other.equals_generic_argument_type_keyword(*this);
}


auto spp::asts::GenericArgumentTypeKeywordAst::equals_generic_argument_type_keyword(
    GenericArgumentTypeKeywordAst const &other) const
    -> std::strong_ordering {
    if (*name == *other.name and *val == *other.val) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentTypeKeywordAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericArgumentTypeKeywordAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::GenericArgumentTypeKeywordAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericArgumentTypeKeywordAst>(
        ast_clone(name),
        ast_clone(tok_assign),
        ast_clone(val));
}


spp::asts::GenericArgumentTypeKeywordAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentTypeKeywordAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::GenericArgumentTypeKeywordAst::from_symbol(
    analyse::scopes::TypeSymbol const &sym)
    -> std::unique_ptr<GenericArgumentTypeKeywordAst> {
    // Extract the value from the symbol's scope, if it exists.
    auto value = sym.scope ? sym.scope->ty_sym->fq_name()->with_convention(ast_clone(sym.convention.get())) : nullptr;

    // Wrap the value into a type argument.
    return std::make_unique<GenericArgumentTypeKeywordAst>(sym.name, nullptr, std::move(value));
}


auto spp::asts::GenericArgumentTypeKeywordAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Analyse the name and value of the generic type argument.
    val->stage_7_analyse_semantics(sm, meta);
    const auto tmp1 = sm->current_scope->get_type_symbol(val);
    const auto tmp2 = tmp1->fq_name();
    auto tmp3 = ast_clone(val->get_convention());
    const auto tmp4 = tmp2->with_convention(std::move(tmp3));
    val = tmp4;
}
