module;
#include <spp/macros.hpp>

module spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;


spp::asts::LocalVariableDestructureSkipSingleArgumentAst::LocalVariableDestructureSkipSingleArgumentAst(
    decltype(tok_underscore) &&tok_underscore) :
    tok_underscore(std::move(tok_underscore)) {
}


spp::asts::LocalVariableDestructureSkipSingleArgumentAst::~LocalVariableDestructureSkipSingleArgumentAst() = default;


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::pos_start() const
    -> std::size_t {
    return tok_underscore->pos_start();
}


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::pos_end() const
    -> std::size_t {
    return tok_underscore->pos_end();
}


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableDestructureSkipSingleArgumentAst>(ast_clone(tok_underscore));
}


spp::asts::LocalVariableDestructureSkipSingleArgumentAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_underscore);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_underscore);
    SPP_PRINT_END;
}


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return std::make_shared<IdentifierAst>(pos_start(), "_UNMATCHABLE");
}


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {};
}
