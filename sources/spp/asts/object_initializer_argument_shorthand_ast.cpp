module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_argument_shorthand_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
auto spp::asts::ObjectInitializerArgumentShorthandAst::CreateAutoFillArg(
    Unique<ExpressionAst> &&val)
    -> Unique<ObjectInitializerArgumentShorthandAst> {
    // Wrap the constructor with some fixed arguments.
    return MakeUnique<ObjectInitializerArgumentShorthandAst>(
        TokenAst::NewEmpty(lex::SppTokenType::TK_DOUBLE_DOT, ".."),
        std::move(val));
}

spp::asts::ObjectInitializerArgumentShorthandAst::ObjectInitializerArgumentShorthandAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Val) &&val) :
    ObjectInitializerArgumentAst(AstClone(val->To<IdentifierAst>()), std::move(val)),
    TokEllipsis(std::move(tok_ellipsis)) {
}

spp::asts::ObjectInitializerArgumentShorthandAst::~ObjectInitializerArgumentShorthandAst() = default;

auto spp::asts::ObjectInitializerArgumentShorthandAst::PosStart() const
    -> std::size_t {
    // Use the ".." token or value.
    return TokEllipsis != nullptr ? TokEllipsis->PosStart() : Val->PosStart();
}

auto spp::asts::ObjectInitializerArgumentShorthandAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::ObjectInitializerArgumentShorthandAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ObjectInitializerArgumentShorthandAst>(
        AstClone(TokEllipsis), AstClone(Val));
}

auto spp::asts::ObjectInitializerArgumentShorthandAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokEllipsis);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::ObjectInitializerArgumentShorthandAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppObjectInitializerInvalidArgumentError;

    // The parser allows Type(123) as a postfix function call over a type, which is invalid as type initialization.
    RaiseIf<SppObjectInitializerInvalidArgumentError>(
        Val->To<IdentifierAst>() == nullptr,
        {sm->CurrentScope}, ERR_ARGS(*this));
    ObjectInitializerArgumentAst::Stage7_AnalyseSemantics(sm, meta);
}

SPP_MOD_END
