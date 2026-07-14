module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::CasePatternVariantDestructureSkipMultipleArgumentsAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    Unique<CasePatternVariantAst> &&binding) :
    TokEllipsis(std::move(tok_ellipsis)),
    Binding(dynamic_unique_cast<CasePatternVariantSingleIdentifierAst>(std::move(binding))) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokEllipsis, lex::SppTokenType::TK_DOUBLE_DOT, "..");
}

spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::~CasePatternVariantDestructureSkipMultipleArgumentsAst() = default;

auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::PosStart() const
    -> std::size_t {
    // Use the ".." token.
    return TokEllipsis->PosStart();
}

auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::PosEnd() const
    -> std::size_t {
    // Use the binding or the ".." token.
    return Binding != nullptr ? Binding->PosEnd() : TokEllipsis->PosEnd();
}

auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CasePatternVariantDestructureSkipMultipleArgumentsAst>(
        AstClone(TokEllipsis), AstClone(Binding));
}

auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokEllipsis);
    SPP_STRING_APPEND(Binding);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::ConvToVar(
    meta::CompilerMetaData *meta)
    -> Unique<LocalVariableAst> {
    // Create the local variable destructure attribute binding AST.
    auto var = MakeUnique<LocalVariableDestructureSkipMultipleArgumentsAst>(
        nullptr, Binding != nullptr ? Binding->ConvToVar(meta) : nullptr);
    var->MarkFromCasePattern();
    return var;
}

SPP_MOD_END
