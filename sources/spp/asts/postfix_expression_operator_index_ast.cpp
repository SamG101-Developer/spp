module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_index_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
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
    ast->_MappedFunc = _MappedFunc;
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
    SPP_STRING_APPEND(TokMut).append(TokMut ? " " : "");
    SPP_STRING_APPEND(Expr);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Already analysed => return early.
    using analyse::errors::SppExpressionAmbiguousIndexableError;
    using analyse::errors::SppExpressionNotIndexableError;
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types_precompiled::INDEX_MUT;
    using generate::common_types_precompiled::INDEX_REF;
    if (_MappedFunc != nullptr) { return; }

    // Determine the left-hand-side type.
    const auto lhs_type = const_shared_cast(
        meta->PostfixExpressionLhs->InferType(sm, meta));

    // Ensure the type superimposes the correct indexing variation.
    const auto &index_type = TokMut != nullptr ? INDEX_MUT : INDEX_REF;

    const auto type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);
    auto sup_types = Vec{lhs_type};
    sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

    const auto index_type_candidates = sup_types
        | genex::views::filter([&sm, &index_type](auto const &sup_type) { return TypeEq(*sup_type->WithoutGenerics(), *index_type, *sm->CurrentScope, *sm->CurrentScope); })
        | genex::to<Vec>();

    RaiseIf<SppExpressionNotIndexableError>(
        index_type_candidates.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, "runtime indexing"));

    RaiseIf<SppExpressionAmbiguousIndexableError>(
        index_type_candidates.Len() > 1, {sm->CurrentScope},
        ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, "runtime indexing"));

    // Create the mapped function for the index operator; create the index argument.
    Unique<FunctionCallArgumentAst> arg = MakeUnique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, std::move(Expr));
    auto arg_group = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, Vec<decltype(arg)>{}, nullptr);
    arg_group->Args.EmplaceBack(std::move(arg));

    // Field name is either "index_ref" or "index_mut", then call it with the argument group (index).
    auto field_name = MakeUnique<IdentifierAst>(PosStart(), TokMut != nullptr ? "index_mut" : "index_ref");
    auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
    auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
    auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(arg_group), nullptr);
    _MappedFunc = MakeShared<PostfixExpressionAst>(std::move(member_access), std::move(func_call));
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the mapped function.
    _MappedFunc->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::InferType(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Forward to the mapped function's return type.
    return _MappedFunc->InferType(sm, meta);
}

SPP_MOD_END
