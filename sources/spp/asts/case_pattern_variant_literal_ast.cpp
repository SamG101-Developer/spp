module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_literal_ast;
import spp.analyse.utils.case_utils;
import spp.asts.convention_ref_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantLiteralAst::CasePatternVariantLiteralAst(
    decltype(Literal) &&literal) :
    CasePatternVariantAst(),
    Literal(std::move(literal)) {
}

spp::asts::CasePatternVariantLiteralAst::~CasePatternVariantLiteralAst() = default;

auto spp::asts::CasePatternVariantLiteralAst::PosStart() const
    -> std::size_t {
    // Use the literal.
    return Literal->PosStart();
}

auto spp::asts::CasePatternVariantLiteralAst::PosEnd() const
    -> std::size_t {
    // Use the literal.
    return Literal->PosEnd();
}

auto spp::asts::CasePatternVariantLiteralAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CasePatternVariantLiteralAst>(
        AstClone(Literal));
}

auto spp::asts::CasePatternVariantLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Literal);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantLiteralAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward analysis into the literal.
    Literal->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::CasePatternVariantLiteralAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the literal.
    Literal->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::CasePatternVariantLiteralAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Transform the pattern into comptime values; all need to be true.
    using analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime;
    auto comptime_transforms = CreateAndAnalysePatternEqCompTime(
        {this}, sm, meta);

    // Return the single result (only one literal will be here).
    meta->CmpResult = std::move(comptime_transforms[0]);
}

auto spp::asts::CasePatternVariantLiteralAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm;
    const auto llvm_master_transform = CreateAndAnalysePatternEqFuncsLlvm(
        {this}, sm, meta, ctx);
    return llvm_master_transform[0];
}

auto spp::asts::CasePatternVariantLiteralAst::ConvToVar(
    CompilerMetaData *)
    -> Unique<LocalVariableAst> {
    // Create the local variable literal binding AST.
    const auto uid = spp::utils::Uid(this);
    auto var_name = MakeShared<IdentifierAst>(PosStart(), uid);
    auto var = MakeUnique<LocalVariableSingleIdentifierAst>(
        nullptr, std::move(var_name), nullptr);
    var->MarkFromCasePattern();
    return var;
}

SPP_MOD_END
