module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_skip_single_argument_ast;
import spp.asts.ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::CasePatternVariantDestructureSkipSingleArgumentAst(
    decltype(TokUnderscore) &&tok_underscore) :
    TokUnderscore(std::move(tok_underscore)) {
}

spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::~CasePatternVariantDestructureSkipSingleArgumentAst() = default;

auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::PosStart() const
    -> std::size_t {
    // Use the "_" token.
    return TokUnderscore->PosStart();
}

auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::PosEnd() const
    -> std::size_t {
    // Use the "_" token.
    return TokUnderscore->PosEnd();
}

auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::Clone() const
    -> Unique<Ast> {
    return MakeUnique<CasePatternVariantDestructureSkipSingleArgumentAst>(
        AstClone(TokUnderscore));
}

auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokUnderscore);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::ConvToVar(
    CompilerMetaData *)
    -> Unique<LocalVariableAst> {
    // Create the local variable destructure attribute binding AST.
    auto var = MakeUnique<LocalVariableDestructureSkipSingleArgumentAst>(nullptr);
    var->MarkFromCasePattern();
    return var;
}

SPP_MOD_END
