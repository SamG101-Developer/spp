module;
#include <spp/macros.hpp>

module spp.asts.function_parameter_required_ast;
import spp.asts.ast;
import spp.asts.local_variable_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
FunctionParameterRequiredAst::FunctionParameterRequiredAst(
  decltype(Var) &&var,
  decltype(TokColon) &&tok_colon,
  decltype(Type) type) :
  FunctionParameterAst(
    std::move(var), std::move(tok_colon), std::move(type),
    utils::OrderableTag::kRequiredParam) {
}

FunctionParameterRequiredAst::~FunctionParameterRequiredAst() = default;

auto FunctionParameterRequiredAst::PosStart() const -> std::size_t {
  // Use the variable.
  return Var->PosStart();
}

auto FunctionParameterRequiredAst::PosEnd() const -> std::size_t {
  // Use the type.
  return Source.OriginalType->PosEnd();
}

auto FunctionParameterRequiredAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<FunctionParameterRequiredAst>(
    AstClone(Var),
    AstClone(TokColon),
    AstCloneShared(Type));
}

auto FunctionParameterRequiredAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Var);
  SPP_STRING_APPEND(TokColon).append(" ");
  SPP_STRING_APPEND(Type);
  SPP_STRING_END;
}

SPP_MOD_END
