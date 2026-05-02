module;
#include <spp/macros.hpp>

module spp.asts.function_call_argument_positional_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::FunctionCallArgumentPositionalAst::FunctionCallArgumentPositionalAst(
    decltype(Conv) &&conv,
    decltype(TokUnpack) &&tok_unpack,
    decltype(Val) &&val) :
    FunctionCallArgumentAst(std::move(conv), std::move(val), utils::OrderableTag::POSITIONAL_ARG),
    TokUnpack(std::move(tok_unpack)) {
}

spp::asts::FunctionCallArgumentPositionalAst::~FunctionCallArgumentPositionalAst() = default;

auto spp::asts::FunctionCallArgumentPositionalAst::PosStart() const
    -> std::size_t {
    // Use the ".." token or the value.
    return TokUnpack ? TokUnpack->PosStart() : Val->PosStart();
}

auto spp::asts::FunctionCallArgumentPositionalAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::FunctionCallArgumentPositionalAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<FunctionCallArgumentPositionalAst>(
        AstClone(Conv), AstClone(TokUnpack), AstClone(Val));
}

auto spp::asts::FunctionCallArgumentPositionalAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokUnpack);
    SPP_STRING_APPEND(Conv);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

SPP_MOD_END
