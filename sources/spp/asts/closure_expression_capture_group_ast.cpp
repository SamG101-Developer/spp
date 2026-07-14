module;
#include <spp/macros.hpp>

module spp.asts.closure_expression_capture_group_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.closure_expression_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.algorithms;
import spp.utils.uid;

SPP_MOD_BEGIN
auto spp::asts::ClosureExpressionCaptureGroupAst::NewEmpty()
    -> Unique<ClosureExpressionCaptureGroupAst> {
    // Empty.
    return MakeUnique<ClosureExpressionCaptureGroupAst>(
        nullptr, decltype(Captures)());
}

spp::asts::ClosureExpressionCaptureGroupAst::ClosureExpressionCaptureGroupAst(
    decltype(TokCaps) &&tok_caps,
    decltype(Captures) &&captures) :
    TokCaps(std::move(tok_caps)),
    Captures(std::move(captures)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCaps, lex::SppTokenType::KW_CAPS, "caps");
}

spp::asts::ClosureExpressionCaptureGroupAst::~ClosureExpressionCaptureGroupAst() = default;

auto spp::asts::ClosureExpressionCaptureGroupAst::PosStart() const
    -> std::size_t {
    // Use the "caps" token.
    return TokCaps->PosStart();
}

auto spp::asts::ClosureExpressionCaptureGroupAst::PosEnd() const
    -> std::size_t {
    // Use the final capture.
    return Captures.Back()->PosEnd();
}

auto spp::asts::ClosureExpressionCaptureGroupAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ClosureExpressionCaptureGroupAst>(
        AstClone(TokCaps), AstCloneVec(Captures));
}

auto spp::asts::ClosureExpressionCaptureGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokCaps).append(" ");
    SPP_STRING_EXTEND(Captures, ", ");
    SPP_STRING_END;
}

auto spp::asts::ClosureExpressionCaptureGroupAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Add the capture variables after analysis, otherwise their symbol checks refer to the new captures, not the
    // original asts from the argument group analysis.
    for (auto const &cap : Captures) {
        // Create a "let" statement to insert the symbol into the current scope.
        auto cap_val = AstClone(cap->Val->To<IdentifierAst>());
        auto var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, AstClone(cap_val), nullptr);
        const auto let = MakeUnique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, AstClone(cap->Val));
        let->Stage7_AnalyseSemantics(sm, meta);

        // Apply the borrow to the symbol.
        const auto sym = sm->CurrentScope->GetVarSymbol(cap->Val->To<IdentifierAst>());
        const auto conv = cap->Conv.Get();
        sym->MemInfo->AstBorrowed = {conv, sm->CurrentScope};
        sym->Type = sym->Type->WithConvention(AstClone(cap->Conv));
    }
}

auto spp::asts::ClosureExpressionCaptureGroupAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Any borrowed captures need pinning and marking as extended borrows.
    for (auto const &cap : Captures) {
        if (cap->Conv != nullptr) {
            // Mark the pins on the capture and the target.
            const auto cap_val = cap->Val->To<IdentifierAst>();
            auto cap_sym = sm->CurrentScope->GetVarSymbol(cap_val);
            cap_sym->MemInfo->AstBorrowed = {cap->Conv.Get(), sm->CurrentScope};
            // if (ass_sym != nullptr) { ass_sym->MemInfo->AstPins.EmplaceBack(cap->Val.Get()); }
            // TODO: New escaping borrow system needs using here

            cap_sym = meta->CurrentLambdaOuterScope->GetVarSymbol(cap_val);
            // cap_sym->MemInfo->AstPins.EmplaceBack(cap->Val.Get());
        }
        else {
            // Mark the symbol from the outer context as moved.
            const auto cap_sym = meta->CurrentLambdaOuterScope->GetVarSymbol(cap->Val->To<IdentifierAst>());
            cap_sym->MemInfo->AstMoved = {this, sm->CurrentScope};
        }
    }
}

auto spp::asts::ClosureExpressionCaptureGroupAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Build the variable bindings from the environment object. This allows the body to remain unchanged as the
    // variables get loaded from the environment struct.
    const auto uid = spp::utils::Uid(this);
    for (auto const &[i, capture] : Captures | std::views::enumerate) {
        const auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 0);
        const auto idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), static_cast<std::uint64_t>(i));

        // For the capture x, mock "let x = env.x".
        const auto cap_val = capture->Val->To<IdentifierAst>();
        auto cap_ty = capture->InferType(sm, meta);
        // const auto cap_ty_sym = sm->CurrentScope->GetTypeSymbol(*cap_ty);
        const auto cap_llvm_type = codegen::llvm_type(
            *sm->CurrentScope->GetTypeSymbol(cap_ty), ctx);

        // Create the alloca for the variable.
        const auto alloca = ctx->Builder.CreateAlloca(
            cap_llvm_type, nullptr, "capture.alloca." + uid);

        const auto gep = ctx->Builder.CreateInBoundsGEP(
            ctx->CurrentClosureType,
            ctx->CurrentClosureScope->AstNode->To<ClosureExpressionAst>()->GetLlvmFunc()->Target->getArg(0),
            Vec<llvm::Value*>{zero, idx}.ToStdVector());

        const auto load = ctx->Builder.CreateLoad(cap_llvm_type, gep, "capture.load." + uid);

        ctx->Builder.CreateStore(load, alloca);

        // Add the alloca to the current scope as a variable symbol.
        // Todo: Handle mutability properly.
        auto var_sym = MakeUnique<analyse::scopes::VariableSymbol>(
            cap_val, AstClone(cap_ty), sm->CurrentScope, false, false);
        var_sym->LlvmInfo->Alloca = alloca;
        sm->CurrentScope->AddVarSymbol(std::move(var_sym));
    }
    return nullptr;
}

SPP_MOD_END
