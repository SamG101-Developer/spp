module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
CasePatternVariantDestructureAttributeBindingAst::CasePatternVariantDestructureAttributeBindingAst(
  decltype(Name) &&name,
  decltype(TokAssign) &&tok_assign,
  decltype(Val) &&val) :
  Name(std::move(name)),
  TokAssign(std::move(tok_assign)),
  Val(std::move(val)) {
}

CasePatternVariantDestructureAttributeBindingAst::~CasePatternVariantDestructureAttributeBindingAst() = default;

auto CasePatternVariantDestructureAttributeBindingAst::PosStart() const -> std::size_t {
  // Use the "name".
  return Name->PosStart();
}

auto CasePatternVariantDestructureAttributeBindingAst::PosEnd() const -> std::size_t {
  // Use the "val".
  return Val->PosEnd();
}

auto CasePatternVariantDestructureAttributeBindingAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<CasePatternVariantDestructureAttributeBindingAst>(
    AstCloneShared(Name),
    AstClone(TokAssign),
    AstClone(Val));
}

auto CasePatternVariantDestructureAttributeBindingAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Name);
  SPP_STRING_APPEND(TokAssign);
  SPP_STRING_APPEND(Val);
  SPP_STRING_END;
}

auto CasePatternVariantDestructureAttributeBindingAst::BindsByMove() const -> bool {
  // "x=<pattern>" and "x as y" bind whatever their value
  // pattern binds.
  return Val != nullptr and Val->BindsByMove();
}

auto CasePatternVariantDestructureAttributeBindingAst::ConvToVar(
  CompilerMetaData *meta) -> Unique<LocalVariableAst> {
  // Create the local variable destructure attribute binding
  // AST.
  auto var = MakeUnique<LocalVariableDestructureAttributeBindingAst>(
    AstCloneShared(Name), AstClone(TokAssign), Val->ConvToVar(meta));
  var->MarkFromCasePattern();
  return var;
}

SPP_MOD_END
