#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/pch.hpp>


spp::asts::ConventionMutAst::ConventionMutAst(
    decltype(tok_borrow) &&tok_borrow,
    decltype(tok_mut) &&tok_mut) :
    ConventionAst(ConventionTag::MUT),
    tok_borrow(std::move(tok_borrow)),
    tok_mut(std::move(tok_mut)) {
}


spp::asts::ConventionMutAst::~ConventionMutAst() = default;


auto spp::asts::ConventionMutAst::pos_start() const -> std::size_t {
    return tok_borrow->pos_start();
}


auto spp::asts::ConventionMutAst::pos_end() const -> std::size_t {
    return tok_mut->pos_end();
}


auto spp::asts::ConventionMutAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ConventionMutAst>(
        ast_clone(tok_borrow),
        ast_clone(tok_mut));
}


spp::asts::ConventionMutAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_borrow);
    SPP_STRING_APPEND(tok_mut);
    SPP_STRING_END;
}


auto spp::asts::ConventionMutAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_borrow);
    SPP_PRINT_APPEND(tok_mut);
    SPP_PRINT_END;
}
