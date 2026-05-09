module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_array_ast;
import spp.lex.tokens;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_array_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantDestructureArrayAst::CasePatternVariantDestructureArrayAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Elems(std::move(elems)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::CasePatternVariantDestructureArrayAst::~CasePatternVariantDestructureArrayAst() = default;

auto spp::asts::CasePatternVariantDestructureArrayAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::CasePatternVariantDestructureArrayAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::CasePatternVariantDestructureArrayAst::Clone() const
    -> Unique<Ast> {
    auto c = MakeUnique<CasePatternVariantDestructureArrayAst>(
        AstClone(TokL), AstCloneVec(Elems), AstClone(TokR));
    c->_MappedLet = AstClone(_MappedLet);
    return c;
}

auto spp::asts::CasePatternVariantDestructureArrayAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Elems, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantDestructureArrayAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsDummyCore;

    // Create the new variable from the pattern in the patterns scope.
    auto var = ConvToVar(meta);
    _MappedLet = MakeUnique<LetStatementInitializedAst>(
        nullptr, std::move(var), nullptr, nullptr, AstClone(meta->CaseCondition));
    _MappedLet->Stage7_AnalyseSemantics(sm, meta);

    // Note there is no nested analysis of "elems", because the "let" statement handles it.
    CreateAndAnalysePatternEqFuncsDummyCore(
        Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto spp::asts::CasePatternVariantDestructureArrayAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checking to the mapped let statement.
    _MappedLet->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::CasePatternVariantDestructureArrayAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Transform the pattern into comptime values; all need to be true.
    using analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime;
    auto comptime_transforms = CreateAndAnalysePatternEqCompTime(
        Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);

    // All must be true for the pattern to match (look for any false).
    const auto all_true = genex::all_of(
        comptime_transforms,
        [](auto const &x) { return x->template To<BooleanLiteralAst>()->IsTrue(); });

    // Generate the "let" statement to introduce all the symbols.
    _MappedLet->Stage9_CompTimeResolve(sm, meta);

    // Based on the result, return the corresponding comptime value.
    const auto p = PosStart();
    meta->CmpResult = all_true ? BooleanLiteralAst::True(p) : BooleanLiteralAst::False(p);
}

auto spp::asts::CasePatternVariantDestructureArrayAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm;

    // Generate the "let" statement to introduce all the symbols.
    if (_MappedLet == nullptr) {
        _MappedLet = MakeUnique<LetStatementInitializedAst>(nullptr, ConvToVar(meta), nullptr, nullptr, AstClone(meta->CaseCondition));
        _MappedLet->Stage7_AnalyseSemantics(sm, meta);
    }
    _MappedLet->Stage11_CodeGen(sm, meta, ctx);

    // Combine all the generated transforms into a single "AND"ed statement.
    auto llvm_transforms = CreateAndAnalysePatternEqFuncsLlvm(
        Elems | genex::views::ptr | genex::to<Vec>(), sm, meta, ctx);
    const auto combine_func = [&ctx](auto *a, auto *b) { return ctx->Builder.CreateAnd(a, b); };
    const auto llvm_master_transform = llvm_transforms.IsEmpty()
        ? dynamic_cast<llvm::Value*>(llvm::ConstantInt::getTrue(*ctx->Context))
        : genex::fold_left_first(llvm_transforms, std::move(combine_func));

    // Return the combined statement.
    return llvm_master_transform;
}

auto spp::asts::CasePatternVariantDestructureArrayAst::ConvToVar(
    CompilerMetaData *meta)
    -> Unique<LocalVariableAst> {
    // Recursively map the elements to their local variable counterparts.
    auto mapped_elems = Elems
        | genex::views::transform([meta](auto const &x) { return x->ConvToVar(meta); })
        | genex::to<Vec>();

    // Create the final local variable wrapping, tag it and return it.
    auto var = MakeUnique<LocalVariableDestructureArrayAst>(nullptr, std::move(mapped_elems), nullptr);
    var->MarkFromCasePattern();
    return var;
}

SPP_MOD_END
