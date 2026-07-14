module;
#include <spp/macros.hpp>

module spp.asts.closure_expression_capture_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::ClosureExpressionCaptureAst::ClosureExpressionCaptureAst(
    decltype(Conv) &&conv,
    decltype(Val) &&val) :
    FunctionCallArgumentPositionalAst(std::move(conv), nullptr, std::move(val)) {
}

spp::asts::ClosureExpressionCaptureAst::~ClosureExpressionCaptureAst() = default;

auto spp::asts::ClosureExpressionCaptureAst::PosStart() const
    -> std::size_t {
    // Use the convention or the value.
    return Conv != nullptr ? Conv->PosStart() : Val->PosStart();
}

auto spp::asts::ClosureExpressionCaptureAst::PosEnd() const
    -> std::size_t {
    // Use the value
    return Val->PosEnd();
}

auto spp::asts::ClosureExpressionCaptureAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ClosureExpressionCaptureAst>(
        AstClone(Conv), AstClone(Val));
}

auto spp::asts::ClosureExpressionCaptureAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Conv);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

SPP_MOD_END
