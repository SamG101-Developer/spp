module;
#include <spp/macros.hpp>

module spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.analyse.utils.destructure_utils;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;


SPP_MOD_BEGIN
spp::asts::LocalVariableDestructureSkipSingleArgumentAst::LocalVariableDestructureSkipSingleArgumentAst(
    decltype(TokUnderscore) &&tok_underscore) :
    TokUnderscore(std::move(tok_underscore)) {
}

spp::asts::LocalVariableDestructureSkipSingleArgumentAst::~LocalVariableDestructureSkipSingleArgumentAst() = default;

auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::PosStart() const
    -> std::size_t {
    // Use the "_" token.
    return TokUnderscore->PosStart();
}

auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::PosEnd() const
    -> std::size_t {
    // Use the "_" token.
    return TokUnderscore->PosEnd();
}

auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LocalVariableDestructureSkipSingleArgumentAst>(
        AstClone(TokUnderscore));
}

auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokUnderscore);
    SPP_STRING_END;
}

auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::ExtractNames() const
    -> Vec<Shared<IdentifierAst>> {
    // There are no names for this "_" single skip.
    return {};
}

auto spp::asts::LocalVariableDestructureSkipSingleArgumentAst::ExtractName() const
    -> Shared<IdentifierAst> {
    // There is no single name for this "_" single skip.
    return analyse::utils::destructure_utils::UnmatchableSingleIdentifier(PosStart());
}

SPP_MOD_END
