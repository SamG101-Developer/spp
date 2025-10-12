#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/local_variable_destructure_attribute_binding_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::LocalVariableDestructureAttributeBindingAst::LocalVariableDestructureAttributeBindingAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    name(std::move(name)),
    tok_assign(std::move(tok_assign)),
    val(std::move(val)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


spp::asts::LocalVariableDestructureAttributeBindingAst::~LocalVariableDestructureAttributeBindingAst() = default;


auto spp::asts::LocalVariableDestructureAttributeBindingAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::LocalVariableDestructureAttributeBindingAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::LocalVariableDestructureAttributeBindingAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableDestructureAttributeBindingAst>(
        ast_clone(name),
        ast_clone(tok_assign),
        ast_clone(val));
}


spp::asts::LocalVariableDestructureAttributeBindingAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name).append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureAttributeBindingAst::print(meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name).append(" ");
    SPP_PRINT_APPEND(tok_assign).append(" ");
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::LocalVariableDestructureAttributeBindingAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return name;
}
