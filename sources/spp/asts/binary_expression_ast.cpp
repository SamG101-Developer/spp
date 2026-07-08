module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.binary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.bin_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.asts.fold_expression_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
spp::asts::BinaryExpressionAst::BinaryExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs) :
    Lhs(std::move(lhs)),
    TokOp(std::move(tok_op)),
    Rhs(std::move(rhs)),
    _MappedFunc(nullptr) {
    Source.OriginalPosStart = Lhs ? Lhs->PosStart() : 0;
    Source.OriginalPosEnd = Rhs ? Rhs->PosEnd() : 0;
}

spp::asts::BinaryExpressionAst::~BinaryExpressionAst() = default;

auto spp::asts::BinaryExpressionAst::PosStart() const
    -> std::size_t {
    // Use the left hand side operand.
    return Lhs ? Lhs->PosStart() : Source.OriginalPosStart;
}

auto spp::asts::BinaryExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the right hand side operand.
    return Rhs ? Rhs->PosEnd() : Source.OriginalPosEnd;
}

auto spp::asts::BinaryExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<BinaryExpressionAst>(
        AstClone(Lhs), AstClone(TokOp), AstClone(Rhs));
    ast->_MappedFunc = _MappedFunc;
    ast->Source = Source;
    return ast;
}

auto spp::asts::BinaryExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    if (Lhs != nullptr) {
        raw_string.append("(");
        SPP_STRING_APPEND(Lhs).append(" ");
        SPP_STRING_APPEND(TokOp).append(" ");
        SPP_STRING_APPEND(Rhs).append(")");
    }
    else {
        SPP_STRING_APPEND(_MappedFunc);
    }
    SPP_STRING_END;
}

auto spp::asts::BinaryExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::utils::bin_utils::ConvertBinExprToFuncCall;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::IsTypeTup;
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppMemberAccessNonIndexableError;
    using analyse::errors::SppInvalidBinaryFoldExpressionError;

    // Todo: this guard shouldn't be needed.
    if (_MappedFunc) { return; }

    // Handle lhs-folding.
    if (Lhs->To<FoldExpressionAst>()) {
        // Check the rhs is a tuple.
        const auto rhs_tuple_type = Rhs->InferType(sm, meta);
        RaiseIf<SppMemberAccessNonIndexableError>(
            not IsTypeTup(*rhs_tuple_type, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*Rhs, *rhs_tuple_type, *Lhs));

        // Get the parts of the tuple.
        const auto rhs_num_elems = rhs_tuple_type->TypeParts()[0]->GnArgGroup->Args.Len();
        RaiseIf<SppInvalidBinaryFoldExpressionError>(
            rhs_num_elems < 2,
            {sm->CurrentScope}, ERR_ARGS(*Rhs, *rhs_tuple_type, rhs_num_elems));

        auto new_asts = UniqueVec<PostfixExpressionAst>();
        for (auto i = 0u; i < rhs_num_elems; ++i) {
            auto field = MakeUnique<IdentifierAst>(Rhs->PosStart(), std::to_string(i));
            auto new_ast = MakeUnique<PostfixExpressionAst>(
                AstClone(Rhs),
                MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            new_ast->Stage7_AnalyseSemantics(sm, meta);
            new_asts.EmplaceBack(std::move(new_ast));
        }

        // Convert "t = (0, 1, 2, 3)", ".. + t" into "(((t.0 + t.1) + t.2) + t.3)".
        Lhs = std::move(new_asts[0]);
        Rhs = std::move(new_asts[1]);
        for (auto &&new_ast : new_asts | genex::views::move | genex::views::drop(2)) {
            Lhs = MakeUnique<BinaryExpressionAst>(std::move(Lhs), AstClone(TokOp), std::move(Rhs));
            Rhs = std::move(new_ast);
        }
        _MappedFunc = ConvertBinExprToFuncCall(*this, sm, meta);
        _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
    }

    // Handle rhs-folding.
    else if (Rhs->To<FoldExpressionAst>()) {
        // Check the lhs is a tuple.
        const auto lhs_tuple_type = Lhs->InferType(sm, meta);
        RaiseIf<SppMemberAccessNonIndexableError>(
            not IsTypeTup(*lhs_tuple_type, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*Lhs, *lhs_tuple_type, *Rhs));

        // Get the parts of the tuple.
        const auto lhs_num_elems = lhs_tuple_type->TypeParts()[0]->GnArgGroup->Args.Len();
        RaiseIf<SppInvalidBinaryFoldExpressionError>(
            lhs_num_elems < 2,
            {sm->CurrentScope}, ERR_ARGS(*Lhs, *lhs_tuple_type, lhs_num_elems));

        auto new_asts = UniqueVec<PostfixExpressionAst>();
        for (auto i = 0u; i < lhs_num_elems; ++i) {
            auto field = MakeUnique<IdentifierAst>(Lhs->PosStart(), std::to_string(i));
            auto new_ast = MakeUnique<PostfixExpressionAst>(
                AstClone(Lhs),
                MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            new_ast->Stage7_AnalyseSemantics(sm, meta);
            new_asts.EmplaceBack(std::move(new_ast));
        }

        // Convert "t = (0, 1, 2, 3)", "t + .." into "(t.0 + (t.1 + (t.2 + t.3)))".
        Lhs = std::move(new_asts[new_asts.Len() - 2]);
        Rhs = std::move(new_asts[new_asts.Len() - 1]);
        for (auto &&new_ast : new_asts | genex::views::move_reverse | genex::views::drop(2)) {
            Rhs = MakeUnique<BinaryExpressionAst>(std::move(Lhs), AstClone(TokOp), std::move(Rhs));
            Lhs = std::move(new_ast);
        }
        _MappedFunc = ConvertBinExprToFuncCall(*this, sm, meta);
        _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
    }

    else {
        // Standard non-folding binary expression.
        _MappedFunc = ConvertBinExprToFuncCall(*this, sm, meta);
        _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
    }
}

auto spp::asts::BinaryExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the memory checking to the mapped function.
    _MappedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::BinaryExpressionAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the compile-time resolution to the mapped function.
    _MappedFunc->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::BinaryExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Forward the code generation to the mapped function.
    return _MappedFunc->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::BinaryExpressionAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Infer the type from the function mapping of the binary expression.
    // if (_MappedFunc == nullptr) {
    //     // Todo: Needed?
    //     Stage7_AnalyseSemantics(sm, meta);
    // }
    return _MappedFunc->InferType(sm, meta);
}

SPP_MOD_END
