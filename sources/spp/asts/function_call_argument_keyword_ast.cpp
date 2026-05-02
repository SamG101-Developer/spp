module;
#include <spp/macros.hpp>

module spp.asts.function_call_argument_keyword_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::FunctionCallArgumentKeywordAst::FunctionCallArgumentKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Conv) &&conv,
    decltype(Val) &&val) :
    FunctionCallArgumentAst(std::move(conv), std::move(val), utils::OrderableTag::KEYWORD_ARG),
    Name(std::move(name)),
    TokAssign(std::move(tok_assign)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

spp::asts::FunctionCallArgumentKeywordAst::~FunctionCallArgumentKeywordAst() = default;

auto spp::asts::FunctionCallArgumentKeywordAst::PosStart() const
    -> std::size_t {
    // Use the name.
    return Name->PosStart();
}

auto spp::asts::FunctionCallArgumentKeywordAst::PosEnd() const
    -> std::size_t {
    // Use the val.
    return Val->PosEnd();
}

auto spp::asts::FunctionCallArgumentKeywordAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<FunctionCallArgumentKeywordAst>(
        AstCloneShared(Name), AstClone(TokAssign), AstClone(Conv), AstClone(Val));
}

auto spp::asts::FunctionCallArgumentKeywordAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokAssign);
    SPP_STRING_APPEND(Conv);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

SPP_MOD_END
