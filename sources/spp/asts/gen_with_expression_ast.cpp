module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.gen_with_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.gen_expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.loop_iterable_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::GenWithExpressionAst::GenWithExpressionAst(
    decltype(TokGen) &&tok_gen,
    decltype(TokWith) &&tok_with,
    decltype(Expr) &&expr) :
    TokGen(std::move(tok_gen)),
    TokWith(std::move(tok_with)),
    Expr(std::move(expr)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokGen, lex::SppTokenType::KW_GEN, "gen");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokWith, lex::SppTokenType::KW_WITH, "with");
}

spp::asts::GenWithExpressionAst::~GenWithExpressionAst() = default;

auto spp::asts::GenWithExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "gen" token.
    return TokGen->PosStart();
}

auto spp::asts::GenWithExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the expression.
    return Expr->PosEnd();
}

auto spp::asts::GenWithExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenWithExpressionAst>(
        AstClone(TokGen), AstClone(TokWith), AstClone(Expr));
}

auto spp::asts::GenWithExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokGen).append(" ");
    SPP_STRING_APPEND(TokWith).append(" ");
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::GenWithExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Todo: Just map this to a loop and individual "gen" inside?
    //
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::GetGenAndYieldTypes;
    using analyse::utils::type_utils::TypeEq;
    using analyse::errors::SppFunctionSubroutineContainsGenExpressionError;
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppTypeMismatchError;
    using generate::common_types::VoidType;

    // Check the enclosing function is a coroutine and not a subroutine.
    const auto function_flavour = meta->EnclosingFunctionFlavour;
    RaiseIf<SppFunctionSubroutineContainsGenExpressionError>(
        function_flavour->TokenType != lex::SppTokenType::KW_COR,
        {sm->CurrentScope}, ERR_ARGS(*function_flavour, *TokGen));

    // Analyse the expression (guaranteed to exist), and determine the type of the expression.
    auto expr_type = VoidType(PosStart());
    meta->Save();
    SPP_RETURN_TYPE_OVERLOAD_HELPER(Expr.get()) {
        meta->ReturnTypeOverloadResolverType = meta->EnclosingFunctionRetType[0];
    }

    meta->AssignmentTargetType = meta->EnclosingFunctionRetType.IsEmpty() ? nullptr : meta->EnclosingFunctionRetType[0];
    meta->AssignmentTarget = IdentifierAst::FromType(*meta->AssignmentTargetType);
    meta->PreventAutoGeneratorResume = true;

    // Analyse the expression.
    Expr->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Expr, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Expr));

    expr_type = Expr->InferType(sm, meta);
    meta->Restore();

    // Functions provide the return type, closures require inference; handle the inference.
    if (meta->EnclosingFunctionRetType.IsEmpty()) {
        _GenType = generate::common_types::GenType(Expr->PosStart(), expr_type);
        _GenType->Stage7_AnalyseSemantics(sm, meta);
        meta->EnclosingFunctionRetType.EmplaceBack(_GenType);
        meta->EnclosingFunctionSourceRetType.EmplaceBack(expr_type);
        meta->EnclosingFunctionScope = sm->CurrentScope;
    }
    else {
        _GenType = meta->EnclosingFunctionRetType[0];
    }

    // Determine the "yield" type of the enclosing function and expression.
    auto [_, yield_type1, _] = GetGenAndYieldTypes(
        *meta->EnclosingFunctionRetType[0], *sm->CurrentScope, *meta->EnclosingFunctionRetType[0], "coroutine");
    auto [_, yield_type2, _] = GetGenAndYieldTypes(
        *expr_type, *sm->CurrentScope, *expr_type, "coroutine");

    RaiseIf<SppTypeMismatchError>(
        not TypeEq(*yield_type1, *yield_type2, *meta->EnclosingFunctionScope, *sm->CurrentScope),
        {meta->EnclosingFunctionScope, sm->CurrentScope},
        ERR_ARGS(*meta->EnclosingFunctionSourceRetType[0], *yield_type1, *Expr, *yield_type2));
}

auto spp::asts::GenWithExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the expression for memory issues.
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Ensure the argument isn't moved or partially moved (for all conventions)
    // Todo: Investigate pin checks here.
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Expr, *TokGen, *sm, true, true, false, false, false, meta);
}

auto spp::asts::GenWithExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Build the iterable for the loop; the iterable is just the expression, mapped into a different AST.
    auto temp_var_name = MakeShared<IdentifierAst>(0uz, "_coro_temp");
    auto temp_var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, std::move(temp_var_name), nullptr);

    // Build the "gen" expression for the individual yielding.
    auto gen_value = MakeUnique<IdentifierAst>(0uz, "_coro_temp");
    auto gen_expression = MakeUnique<GenExpressionAst>(nullptr, nullptr, std::move(gen_value));
    auto loop_body = MakeUnique<decltype(LoopIterableExpressionAst::Body)::element_type>(
        nullptr, Vec<Unique<StatementAst>>(), nullptr);
    loop_body->Members.EmplaceBack(Unique<StatementAst>(gen_expression.release()));

    // Build the loop expression with the iterable condition.
    const auto loop_expr = MakeUnique<LoopIterableExpressionAst>(
        nullptr, std::move(temp_var), nullptr, std::move(Expr), std::move(loop_body), nullptr);

    // Analyse it to transform into the correct form (conditional loop).
    const auto current_scope = sm->CurrentScope;
    auto iter_copy = sm->CurrentIterator();
    loop_expr->Stage7_AnalyseSemantics(sm, meta);
    sm->Reset(current_scope, iter_copy);

    // Reset the scope to before the loop and then generate it.
    return loop_expr->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::GenWithExpressionAst::InferType(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // Get the "Send" generic type parameter from the generator type.
    auto send_type = _GenType->TypeParts().Back()->GnArgGroup->TypeAt("Send")->Val;
    return send_type;
}

SPP_MOD_END
