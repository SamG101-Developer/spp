module;
#include <spp/macros.hpp>

module spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.analyse.utils.destructure_utils;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;


SPP_MOD_BEGIN
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


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    // There are no names for this "_" single skip.
    return {};
}


auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    // There is no single name for this "_" single skip.
    return analyse::utils::destructure_utils::unmatchable_single_identifier(pos_start());
}

SPP_MOD_END
