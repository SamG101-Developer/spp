#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/local_variable_destructure_skip_multiple_arguments_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::LocalVariableDestructureSkipMultipleArgumentsAst(
    decltype(tok_ellipsis) &&tok_ellipsis,
    std::unique_ptr<LocalVariableAst> &&binding) :
    tok_ellipsis(std::move(tok_ellipsis)),
    binding(ast_cast<LocalVariableSingleIdentifierAst>(std::move(binding))) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_ellipsis, lex::SppTokenType::TK_DOUBLE_DOT, "..");
}


spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::~LocalVariableDestructureSkipMultipleArgumentsAst() = default;


auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::pos_start() const
    -> std::size_t {
    return tok_ellipsis->pos_start();
}


auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::pos_end() const
    -> std::size_t {
    return binding ? binding->pos_end() : tok_ellipsis->pos_end();
}


auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableDestructureSkipMultipleArgumentsAst>(
        ast_clone(tok_ellipsis),
        ast_clone(binding));
}


spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_APPEND(binding);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_APPEND(binding);
    SPP_PRINT_END;
}


auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return std::make_shared<IdentifierAst>(pos_start(), "_UNMATCHABLE");
}


auto spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return binding != nullptr ? binding->extract_names() : std::vector<std::shared_ptr<IdentifierAst>>();
}
