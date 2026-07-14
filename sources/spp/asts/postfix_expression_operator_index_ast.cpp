module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_index_ast;
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

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorIndexAst::PostfixExpressionOperatorIndexAst(
    Unique<TokenAst> &&tok_l,
    Unique<TokenAst> &&tok_mut,
    Unique<ExpressionAst> &&expr,
    Unique<TokenAst> &&tok_r) :
    TokL(std::move(tok_l)),
    TokMut(std::move(tok_mut)),
    Expr(std::move(expr)),
    TokR(std::move(tok_r)),
    _MappedFunc(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::PostfixExpressionOperatorIndexAst::~PostfixExpressionOperatorIndexAst() = default;

auto spp::asts::PostfixExpressionOperatorIndexAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::PostfixExpressionOperatorIndexAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<PostfixExpressionOperatorIndexAst>(
        AstClone(TokL), AstClone(TokMut), AstClone(Expr), AstClone(TokR));
    ast->_MappedFunc = AstClone(_MappedFunc);
    return ast;
}

auto spp::asts::PostfixExpressionOperatorIndexAst::ToString() const
    -> Str {
    SPP_STRING_START;
    if (_MappedFunc != nullptr) {
        SPP_STRING_APPEND(_MappedFunc->Op);
        SPP_STRING_END;
    }
    SPP_STRING_APPEND(TokL);
    SPP_STRING_APPEND(TokMut).append(TokMut != nullptr ? " " : "");
    SPP_STRING_APPEND(Expr);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Already analysed => return early.
    using analyse::errors::SppExpressionAmbiguousIndexableError;
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::TypeEq;
    if (_MappedFunc != nullptr) { return; }

    // Determine the left-hand-side type.
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);

    const auto type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);
    auto sup_types = type_sym->LinkedScope->SupTypes();
    sup_types.EmplaceBack(lhs_type);

    // Create the mapped function for the index operator; create the index argument.
    Unique<FunctionCallArgumentAst> arg = MakeUnique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, std::move(Expr));
    auto arg_group = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, Vec<decltype(arg)>{}, nullptr);
    arg_group->Args.EmplaceBack(std::move(arg));

    // Field name is either "index_ref" or "index_mut", then call it with the argument group (index).
    auto field_name = MakeUnique<IdentifierAst>(PosStart(), TokMut != nullptr ? "index_mut" : "index_ref");
    auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
    auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
    auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(arg_group), nullptr);
    func_call->Source.OriginalExpr = this;
    _MappedFunc = MakeUnique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to the mapped function.
    _MappedFunc->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Forward to the mapped function's return type.
    const auto inferred = _MappedFunc->InferType(sm, meta);
    CACHE_TYPE_INFERENCE_AND_RETURN(AstClone(inferred));
}

SPP_MOD_END
