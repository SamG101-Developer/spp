module;
#include <spp/macros.hpp>

module spp.asts.object_initializer_argument_shorthand_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.expression_ast;


spp::asts::ObjectInitializerArgumentShorthandAst::ObjectInitializerArgumentShorthandAst(
    std::unique_ptr<TokenAst> tok_ellipsis,
    std::unique_ptr<ExpressionAst> &&val) :
    ObjectInitializerArgumentAst(ast_cast<IdentifierAst>(ast_clone(val)), std::move(val)),
    tok_ellipsis(std::move(tok_ellipsis)) {
}


spp::asts::ObjectInitializerArgumentShorthandAst::~ObjectInitializerArgumentShorthandAst() = default;


auto spp::asts::ObjectInitializerArgumentShorthandAst::pos_start() const
    -> std::size_t {
    return tok_ellipsis ? tok_ellipsis->pos_start() : val->pos_start();
}


auto spp::asts::ObjectInitializerArgumentShorthandAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::ObjectInitializerArgumentShorthandAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ObjectInitializerArgumentShorthandAst>(
        ast_clone(tok_ellipsis),
        ast_clone(val));
}


spp::asts::ObjectInitializerArgumentShorthandAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::ObjectInitializerArgumentShorthandAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::ObjectInitializerArgumentShorthandAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta) -> void {
    // The parser allows Type(123) as a postfix function call over a type, which is invalid as type initialization.
    if (ast_cast<IdentifierAst>(val.get()) == nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppObjectInitializerInvalidArgumentError>().with_args(
            *this).with_scopes({sm->current_scope}).raise();
    }
    ObjectInitializerArgumentAst::stage_7_analyse_semantics(sm, meta);
}
