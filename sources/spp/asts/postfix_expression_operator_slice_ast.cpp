module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_slice_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorSliceAst::PostfixExpressionOperatorSliceAst(
    Unique<TokenAst> &&tok_l,
    Unique<TokenAst> &&tok_mut,
    Unique<ExpressionAst> &&expr_l_bound,
    Unique<TokenAst> &&tok_to,
    Unique<ExpressionAst> &&expr_r_bound,
    Unique<TokenAst> &&tok_r) :
    TokL(std::move(tok_l)),
    TokMut(std::move(tok_mut)),
    ExprLBound(std::move(expr_l_bound)),
    TokTo(std::move(tok_to)),
    ExprRBound(std::move(expr_r_bound)),
    TokR(std::move(tok_r)),
    _MappedFunc(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokTo, lex::SppTokenType::KW_TO, "to");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::PostfixExpressionOperatorSliceAst::~PostfixExpressionOperatorSliceAst() = default;

auto spp::asts::PostfixExpressionOperatorSliceAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::PostfixExpressionOperatorSliceAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorSliceAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<PostfixExpressionOperatorSliceAst>(
        AstClone(TokL), AstClone(TokMut), AstClone(ExprLBound), AstClone(TokTo), AstClone(ExprRBound), AstClone(TokR));
    ast->_MappedFunc = _MappedFunc;
    return ast;
}

auto spp::asts::PostfixExpressionOperatorSliceAst::ToString() const
    -> Str {
    SPP_STRING_START;
    if (_MappedFunc != nullptr) {
        SPP_STRING_APPEND(_MappedFunc->Op);
        SPP_STRING_END;
    }
    SPP_STRING_APPEND(TokL);
    SPP_STRING_APPEND(TokMut).append(TokMut ? " " : "");
    SPP_STRING_APPEND(ExprLBound);
    SPP_STRING_APPEND(TokTo).append(" ");
    SPP_STRING_APPEND(ExprRBound);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorSliceAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Already analysed => return early.
    using analyse::errors::SppExpressionAmbiguousIndexableError;
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types_precompiled::SLICE_MUT;
    using generate::common_types_precompiled::SLICE_REF;
    if (_MappedFunc != nullptr) { return; }

    // Determine the left-hand-side type.
    const auto lhs_type = const_shared_cast(
        meta->PostfixExpressionLhs->InferType(sm, meta));

    const auto type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);
    auto sup_types = Vec{lhs_type};
    sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

    // Create the mapped function for the index operator; create the index argument.
    Unique<FunctionCallArgumentAst> arg1 = MakeUnique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, std::move(ExprLBound));
    Unique<FunctionCallArgumentAst> arg2 = MakeUnique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, std::move(ExprRBound));
    auto arg_group = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, Vec<Unique<FunctionCallArgumentAst>>{}, nullptr);
    arg_group->Args.EmplaceBack(std::move(arg1));
    arg_group->Args.EmplaceBack(std::move(arg2));

    // Field name is either "index_ref" or "index_mut", then call it with the argument group (index).
    auto field_name = MakeUnique<IdentifierAst>(PosStart(), TokMut != nullptr ? "slice_mut" : "slice_ref");
    auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
    auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
    auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(arg_group), nullptr);
    func_call->Source.OriginalExpr = this;
    _MappedFunc = MakeShared<PostfixExpressionAst>(std::move(member_access), std::move(func_call));
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorSliceAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the mapped function.
    _MappedFunc->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorSliceAst::InferType(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Forward to the mapped function's return type.
    return _MappedFunc->InferType(sm, meta);
}

SPP_MOD_END
