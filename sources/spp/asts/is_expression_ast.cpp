module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.is_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.bin_utils;
import spp.analyse.utils.expr_utils;
import spp.asts.case_expression_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::IsExpressionAst::IsExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs) :
    Lhs(std::move(lhs)),
    TokOp(std::move(tok_op)),
    Rhs(std::move(rhs)),
    _MappedFunc(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokOp, lex::SppTokenType::KW_IS, "is");
    Source.OriginalPosStart = Lhs ? Lhs->PosStart() : 0;
    Source.OriginalPosEnd = Rhs ? Rhs->PosEnd() : 0;
}

spp::asts::IsExpressionAst::~IsExpressionAst() = default;

auto spp::asts::IsExpressionAst::PosStart() const
    -> std::size_t {
    // Use the lhs, or the mapped function.
    return Lhs ? Lhs->PosStart() : Source.OriginalPosStart;
}

auto spp::asts::IsExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the rhs, or the mapped function.
    return Rhs ? Rhs->PosEnd() : Source.OriginalPosEnd;
}

auto spp::asts::IsExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<IsExpressionAst>(
        AstClone(Lhs), AstClone(TokOp), AstClone(Rhs));
    ast->_MappedFunc = _MappedFunc;
    return ast;
}

auto spp::asts::IsExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    if (_MappedFunc) {
        SPP_STRING_APPEND(_MappedFunc);
        SPP_STRING_END;
    }
    SPP_STRING_APPEND(Lhs);
    SPP_STRING_APPEND(TokOp);
    SPP_STRING_APPEND(Rhs);
    SPP_STRING_END;
}

auto spp::asts::IsExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::bin_utils::ConvertIsExprToFuncCall;
    using analyse::errors::SppInvalidPrimaryExpressionError;

    _LhsAsId = AstClone(Lhs->To<IdentifierAst>());

    // Convert to a "case" destructure and analyse it.
    const auto n = sm->CurrentScope->Children.Len();
    _MappedFunc = ConvertIsExprToFuncCall(*this, sm, meta);
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);

    // Add the destructure symbols to the current scope.
    // This includes the lhs symbol if it's been flow typed.
    if (not sm->CurrentScope->NameAsString().starts_with("<inner-scope#")) {
        const auto destructure_syms = sm->CurrentScope->Children[n]->Children[0]->AllVarSymbols(true, true);
        for (auto const &x : destructure_syms) { sm->CurrentScope->AddVarSymbol(x); }
    }
}

auto spp::asts::IsExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the memory checking to the mapped function.
    _MappedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::IsExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // If the lhs was an identifier, the "is" causes it to get flow types, so we need to promote the original "alloca"
    // into the flow typed symbol.
    if (_LhsAsId) {
        const auto flow_typed_lhs_sym = sm->CurrentScope->GetVarSymbol(_LhsAsId, true);
        if (flow_typed_lhs_sym != nullptr) {
            auto original_sym = sm->CurrentScope->Parent->GetVarSymbol(_LhsAsId);
            original_sym = original_sym ? original_sym : flow_typed_lhs_sym;
            flow_typed_lhs_sym->LlvmInfo->Alloca = original_sym->LlvmInfo->Alloca;
        }
    }

    // Forward the code generation to the mapped function.
    return _MappedFunc->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::IsExpressionAst::InferType(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // Always return a boolean type (successful or failed match).
    return generate::common_types::BooleanType(_MappedFunc->PosStart());
}

auto spp::asts::IsExpressionAst::GetMappedFunc() const
    -> Shared<CaseExpressionAst> {
    return _MappedFunc;
}

SPP_MOD_END
