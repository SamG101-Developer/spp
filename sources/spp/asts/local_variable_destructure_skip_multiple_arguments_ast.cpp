module;
#include <spp/macros.hpp>

module spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.analyse.utils.destructure_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ptr;

SPP_MOD_BEGIN
spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::LocalVariableDestructureSkipMultipleArgumentsAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    Unique<LocalVariableAst> &&binding) :
    TokEllipsis(std::move(tok_ellipsis)),
    Binding(dynamic_unique_cast<LocalVariableSingleIdentifierAst>(std::move(binding))) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokEllipsis, lex::SppTokenType::TK_DOUBLE_DOT, "..");
}

spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::~LocalVariableDestructureSkipMultipleArgumentsAst() = default;

auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::PosStart() const
    -> std::size_t {
    // Use the ".." token.
    return TokEllipsis->PosStart();
}

auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::PosEnd() const
    -> std::size_t {
    // Use the binding or the ".." token.
    return Binding != nullptr ? Binding->PosEnd() : TokEllipsis->PosEnd();
}

auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LocalVariableDestructureSkipMultipleArgumentsAst>(
        AstClone(TokEllipsis), AstClone(Binding));
}

auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokEllipsis);
    SPP_STRING_APPEND(Binding);
    SPP_STRING_END;
}

auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::ExtractNames() const
    -> Vec<IdentifierAst*> {
    // If there is a binding, use it, otherwise there are no names for this.
    return Binding != nullptr ? Binding->ExtractNames() : Vec<IdentifierAst*>();
}

auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::ExtractName() const
    -> IdentifierAst* {
    // If there is a binding, use it, otherwise this is unmatchable.
    return Binding != nullptr
        ? Binding->ExtractName()
        : analyse::utils::destructure_utils::UnmatchableSingleIdentifier();
}

SPP_MOD_END
