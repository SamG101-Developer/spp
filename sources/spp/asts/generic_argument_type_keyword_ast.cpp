#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>


spp::asts::GenericArgumentTypeKeywordAst::GenericArgumentTypeKeywordAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    GenericArgumentTypeAst(std::move(val)),
    name(std::move(name)),
    tok_assign(std::move(tok_assign)) {
}


spp::asts::GenericArgumentTypeKeywordAst::~GenericArgumentTypeKeywordAst() = default;


auto spp::asts::GenericArgumentTypeKeywordAst::from_symbol(
    analyse::scopes::TypeSymbol const *sym)
    -> std::unique_ptr<GenericArgumentTypeKeywordAst> {
    // Extract the value from the symbol's scope, if it exists.
    auto value = sym->scope
        ? dynamic_cast<analyse::scopes::TypeSymbol*>(sym->scope->ty_sym)->fq_name()->with_convention(ast_clone(sym->convention))
        : nullptr;

    // Wrap the value into a type argument.
    return std::make_unique<GenericArgumentTypeKeywordAst>(sym->name, nullptr, std::move(value));
}


auto spp::asts::GenericArgumentTypeKeywordAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericArgumentTypeKeywordAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


auto spp::asts::GenericArgumentTypeKeywordAst::clone() const -> std::unique_ptr<Ast> {
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


auto spp::asts::GenericArgumentTypeKeywordAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::GenericArgumentTypeKeywordAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the name and value of the generic type argument.
    val->stage_7_analyse_semantics(sm, meta);
    val = sm->current_scope->get_type_symbol(*val)->fq_name()->with_convention(ast_clone(val->get_convention()));
}
