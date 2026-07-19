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
import spp.utils.uid;

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
    if (_MappedLoop != nullptr) {
        SPP_STRING_APPEND(_MappedLoop);
        SPP_STRING_END;
    }
    SPP_STRING_APPEND(TokGen).append(" ");
    SPP_STRING_APPEND(TokWith).append(" ");
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::GenWithExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppFunctionSubroutineContainsGenExpressionError;

    // Check the enclosing function is a coroutine and not a subroutine (kept explicit so the error points at this
    // "gen with", rather than at the synthetic inner "gen").
    const auto function_flavour = meta->EnclosingFunctionFlavour;
    RaiseIf<SppFunctionSubroutineContainsGenExpressionError>(
        function_flavour->TokenType != lex::SppTokenType::KW_COR,
        {sm->CurrentScope}, ERR_ARGS(*function_flavour, *TokGen));

    // Desugar "gen with <Expr>" into "loop _tmp in <Expr> { gen _tmp }". This keeps all "gen" analysis uniform.
    const auto uid = spp::utils::Uid(this);
    auto temp_var = MakeUnique<LocalVariableSingleIdentifierAst>(
        nullptr, MakeShared<IdentifierAst>(PosStart(), "$gen_with_" + uid), nullptr);
    auto gen_value = MakeUnique<IdentifierAst>(PosStart(), "$gen_with_" + uid);
    auto gen_expression = MakeUnique<GenExpressionAst>(nullptr, nullptr, std::move(gen_value));
    auto loop_body = InnerScopeExpressionAst::NewEmpty();
    loop_body->Members.EmplaceBack(std::move(gen_expression));
    _MappedLoop = MakeUnique<LoopIterableExpressionAst>(
        nullptr, std::move(temp_var), nullptr, std::move(Expr), std::move(loop_body), nullptr);

    // Analyse the desugared loop. The inner "gen" populates the enclosing coroutine's return type (for closures) and
    // type-checks the sub-generator's "Yield" against the enclosing coroutine's "Yield".
    _MappedLoop->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::GenWithExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the memory check to the desugared loop (which checks the sub-generator expression and the inner "gen").
    _MappedLoop->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::GenWithExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the desugared loop, already built and analysed in Stage7 (its scope aligns with the walk here).
    return _MappedLoop->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
