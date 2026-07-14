module;
#include <spp/macros.hpp>

module spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::ObjectInitializerArgumentKeywordAst::ObjectInitializerArgumentKeywordAst(
    decltype(Name) &&name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val) :
    ObjectInitializerArgumentAst(std::move(name), std::move(val)),
    TokAssign(std::move(tok_assign)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

spp::asts::ObjectInitializerArgumentKeywordAst::~ObjectInitializerArgumentKeywordAst() = default;

auto spp::asts::ObjectInitializerArgumentKeywordAst::PosStart() const
    -> std::size_t {
    // Use the name.
    return Name->PosStart();
}

auto spp::asts::ObjectInitializerArgumentKeywordAst::PosEnd() const
    -> std::size_t {
    // Use the val.
    return Val->PosEnd();
}

auto spp::asts::ObjectInitializerArgumentKeywordAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ObjectInitializerArgumentKeywordAst>(
        AstClone(Name), AstClone(TokAssign), AstClone(Val));
}

auto spp::asts::ObjectInitializerArgumentKeywordAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokAssign);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

SPP_MOD_END
