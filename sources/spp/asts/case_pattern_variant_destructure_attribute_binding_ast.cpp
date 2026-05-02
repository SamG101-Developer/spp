module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;


SPP_MOD_BEGIN
spp::asts::CasePatternVariantDestructureAttributeBindingAst::CasePatternVariantDestructureAttributeBindingAst(
    decltype(Name) &&name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val) :
    Name(std::move(name)),
    TokAssign(std::move(tok_assign)),
    Val(std::move(val)) {
}

spp::asts::CasePatternVariantDestructureAttributeBindingAst::~CasePatternVariantDestructureAttributeBindingAst() = default;

auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::PosStart() const
    -> std::size_t {
    // Use the "name".
    return Name->PosStart();
}

auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::PosEnd() const
    -> std::size_t {
    // Use the "val".
    return Val->PosEnd();
}

auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CasePatternVariantDestructureAttributeBindingAst>(
        AstCloneShared(Name), AstClone(TokAssign), AstClone(Val));
}

auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokAssign);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::ConvToVar(
    CompilerMetaData *meta)
    -> Unique<LocalVariableAst> {
    // Create the local variable destructure attribute binding AST.
    auto var = MakeUnique<LocalVariableDestructureAttributeBindingAst>(
        AstCloneShared(Name), nullptr, Val->ConvToVar(meta));
    var->MarkFromCasePattern();
    return var;
}

SPP_MOD_END
