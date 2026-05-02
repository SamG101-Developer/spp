module;
#include <spp/macros.hpp>

module spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::LocalVariableDestructureAttributeBindingAst::LocalVariableDestructureAttributeBindingAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val) :
    Name(std::move(name)),
    TokAssign(std::move(tok_assign)),
    Val(std::move(val)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

spp::asts::LocalVariableDestructureAttributeBindingAst::~LocalVariableDestructureAttributeBindingAst() = default;

auto spp::asts::LocalVariableDestructureAttributeBindingAst::PosStart() const
    -> std::size_t {
    // Use the name.
    return Name->PosStart();
}

auto spp::asts::LocalVariableDestructureAttributeBindingAst::PosEnd() const
    -> std::size_t {
    // Use the val.
    return Val->PosEnd();
}

auto spp::asts::LocalVariableDestructureAttributeBindingAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LocalVariableDestructureAttributeBindingAst>(
        AstCloneShared(Name), AstClone(TokAssign), AstClone(Val));
}

auto spp::asts::LocalVariableDestructureAttributeBindingAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name).append(" ");
    SPP_STRING_APPEND(TokAssign).append(" ");
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::LocalVariableDestructureAttributeBindingAst::ExtractName() const
    -> Shared<IdentifierAst> {
    // Return the direct name of this attribute binding => this is the attribute being bound.
    return Name;
}

SPP_MOD_END
