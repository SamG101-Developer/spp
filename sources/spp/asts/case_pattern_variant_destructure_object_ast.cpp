module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.lex.tokens;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantDestructureObjectAst::CasePatternVariantDestructureObjectAst(
    decltype(Type) type,
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r) :
    Type(std::move(type)),
    TokL(std::move(tok_l)),
    Elems(std::move(elems)),
    TokR(std::move(tok_r)),
    _CondSym(nullptr),
    _FlowSym(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
    Source.OriginalType = AstClone(Type);
}

spp::asts::CasePatternVariantDestructureObjectAst::~CasePatternVariantDestructureObjectAst() = default;

auto spp::asts::CasePatternVariantDestructureObjectAst::FromType(
    Shared<TypeAst> const &type)
    -> Unique<CasePatternVariantDestructureObjectAst> {
    auto empty_elems = Vec<Unique<CasePatternVariantAst>>{};
    return MakeUnique<CasePatternVariantDestructureObjectAst>(type, nullptr, std::move(empty_elems), nullptr);
}

auto spp::asts::CasePatternVariantDestructureObjectAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return Source.OriginalType->PosStart();
}

auto spp::asts::CasePatternVariantDestructureObjectAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::CasePatternVariantDestructureObjectAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto c = MakeUnique<CasePatternVariantDestructureObjectAst>(
        AstClone(Type), AstClone(TokL), AstCloneVec(Elems), AstClone(TokR));
    c->_MappedLet = AstClone(_MappedLet);
    return c;
}

auto spp::asts::CasePatternVariantDestructureObjectAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Type);
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Elems, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantDestructureObjectAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsDummyCore;
    using analyse::utils::type_utils::IsTypeVariant;
    using analyse::utils::type_utils::TypeEq;
    using analyse::errors::SppTypeMismatchError;

    // TODO: Move flow typing logic to local variable mapping
    // TODO: Flow typing doesn't work on nested variant destructuring
    // Analyse the class type (required for flow typing).
    auto conv = AstClone(Type->GetConvention());
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type)->FqName();
    Type = Type->WithConvention(std::move(conv));

    // Get the condition symbol if it exists.
    auto cond = meta->CaseCondition;
    _CondSym = sm->CurrentScope->GetVarSymbol(AstCloneShared(cond->To<IdentifierAst>()));
    if (_CondSym == nullptr) {
        auto cond_type = cond->InferType(sm, meta);

        // Create a variable and let statement for the condition.
        const auto uid = spp::utils::Uid(this);
        auto var_name = MakeShared<IdentifierAst>(PosStart(), uid);
        auto var_ast = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, var_name, nullptr);
        const auto let_ast = MakeUnique<LetStatementInitializedAst>(nullptr, std::move(var_ast), cond_type, nullptr, AstClone(cond));

        // TODO: Same logic for array and tuple destructures?
        SPP_DEREF_ALLOW_MOVE_HELPER(let_ast->Val) {
            meta->Save();
            meta->AllowMoveDeref = true;
            let_ast->Stage7_AnalyseSemantics(sm, meta);
            meta->Restore();
        }
        else {
            let_ast->Stage7_AnalyseSemantics(sm, meta);
        }

        // Set the memory information of the symbol based on the type of iteration.
        cond = var_name.get();
        _CondSym = sm->CurrentScope->GetVarSymbol(var_name);
    }

    // Flow type the condition symbol if necessary.
    if (IsTypeVariant(*_CondSym->Type, *sm->CurrentScope)) {
        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*_CondSym->Type, *Type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*cond, *_CondSym->Type, *Source.OriginalType, *Type));

        _FlowSym = MakeShared<analyse::scopes::VariableSymbol>(*_CondSym);
        _FlowSym->Type = Type;
        sm->CurrentScope->AddVarSymbol(_FlowSym);
    }

    // Create the new variable from the pattern in the patterns scope.
    auto var = ConvToVar(meta);
    _MappedLet = MakeUnique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, AstClone(cond));
    _MappedLet->Stage7_AnalyseSemantics(sm, meta);

    // Note there is no nested analysis of "elems", because the "let" statement handles it.
    CreateAndAnalysePatternEqFuncsDummyCore(
        Elems | genex::views::ptr | genex::to<Vec>(), sm, meta);
}

auto spp::asts::CasePatternVariantDestructureObjectAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checking to the mapped let statement.
    _MappedLet->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::CasePatternVariantDestructureObjectAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime;

    // TODO: Do a non-variant type comparison first.
    // TODO: Do not allow if the condition type is variant.
    // Transform the pattern into comptime values; all need to be true.
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

auto spp::asts::CasePatternVariantDestructureObjectAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm;

    // Attach the alloca to the potential flow symbol from the outer version of it.
    if (_FlowSym and _CondSym) {
        _FlowSym->LlvmInfo->Alloca = _CondSym->LlvmInfo->Alloca;
    }

    // Generate the "let" statement to introduce all the symbols.
    if (_MappedLet == nullptr) {
        auto var = ConvToVar(meta);
        _MappedLet = MakeUnique<LetStatementInitializedAst>(
            nullptr, std::move(var), nullptr, nullptr, AstClone(meta->CaseCondition));
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

auto spp::asts::CasePatternVariantDestructureObjectAst::ConvToVar(
    CompilerMetaData *meta)
    -> Unique<LocalVariableAst> {
    // Recursively map the elements to their local variable counterparts.
    auto mapped_elems = Elems
        | genex::views::transform([meta](auto const &x) { return x->ConvToVar(meta); })
        | genex::to<Vec>();

    // Create the final local variable wrapping, tag it and return it.
    auto var = MakeUnique<LocalVariableDestructureObjectAst>(
        AstCloneShared(Type), nullptr, std::move(mapped_elems), nullptr);
    var->MarkFromCasePattern();
    return var;
}

SPP_MOD_END
