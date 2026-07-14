module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.closure_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.type_identifier_ast;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import spp.utils.algorithms;

SPP_MOD_BEGIN
spp::asts::ClosureExpressionAst::ClosureExpressionAst(
    decltype(Tok) &&tok,
    decltype(PcGroup) &&pc_group,
    decltype(Body) &&body) :
    Tok(std::move(tok)),
    PcGroup(std::move(pc_group)),
    Body(std::move(body)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Tok, lex::SppTokenType::KW_FUN, "fun");
    Source._OriginalRetType = nullptr;
    _RetType = nullptr;
}

spp::asts::ClosureExpressionAst::~ClosureExpressionAst() = default;

auto spp::asts::ClosureExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "cor" token if present, otherwise the pc group.
    return Tok != nullptr ? Tok->PosStart() : PcGroup->PosStart();
}

auto spp::asts::ClosureExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the body.
    return Body->PosEnd();
}

auto spp::asts::ClosureExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto c = MakeUnique<ClosureExpressionAst>(
        AstClone(Tok), AstClone(PcGroup), AstClone(Body));
    c->_RetType = AstClone(_RetType);
    return c;
}

auto spp::asts::ClosureExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Tok).append(" ");
    SPP_STRING_APPEND(PcGroup).append(" ");
    SPP_STRING_APPEND(Body);
    SPP_STRING_END;
}

auto spp::asts::ClosureExpressionAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Save the current scope for later resetting.
    const auto parent_scope = sm->CurrentScope;
    meta->Save();
    meta->OverriddenScopeForClosure = parent_scope;
    PcGroup->Stage7_AnalyseSemantics(sm, meta);

    const auto inherited_type_generics = sm->CurrentScope->AllTypeSymbols()
        | std::views::filter([](auto const &sym) { return sym->IsGeneric; })
        | std::ranges::to<Vec>();

    const auto inherited_comp_generics = sm->CurrentScope->AllVarSymbols()
        | std::views::filter([](auto const &sym) { return sym->IsGeneric; })
        | std::ranges::to<Vec>();

    // Update the meta args with the closure information for body analysis.
    // The closure-wide save/restore allows for the "ret" to match the closure's inferred return type.
    meta->Save();
    meta->EnclosingFunctionScope = sm->CurrentScope; // this will be the closure-outer scope
    sm->CurrentScope->Parent = sm->CurrentScope->ParentModule();

    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "closure-inner", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    meta->EnclosingFunctionFlavour = Tok.Get();
    meta->EnclosingFunctionRetType.Clear();
    meta->EnclosingFunctionSourceRetType = {};

    // Add the inherited generics into the closure-inner scope.
    for (auto const &type_generic_sym : inherited_type_generics) {
        sm->CurrentScope->AddTypeSymbol(MakeUnique<analyse::scopes::TypeSymbol>(*type_generic_sym));
    }
    for (auto const &comp_generic_sym : inherited_comp_generics) {
        sm->CurrentScope->AddVarSymbol(MakeUnique<analyse::scopes::VariableSymbol>(*comp_generic_sym));
    }

    // Analyse the body of the closure.
    Body->Stage7_AnalyseSemantics(sm, meta);
    _RetType = AstClone(not meta->EnclosingFunctionRetType.IsEmpty() ? meta->EnclosingFunctionRetType[0].Get() : Body->InferType(sm, meta));
    Source._OriginalRetType = AstClone(_RetType);
    meta->Restore(true);
    meta->Restore();

    // Set the scope back.
    sm->CurrentScope = parent_scope;
}

auto spp::asts::ClosureExpressionAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Save the current scope for later resetting.
    const auto parent_scope = sm->CurrentScope;
    meta->Save();
    PcGroup->Stage8_CheckMemory(sm, meta);

    // Prevent the body inheriting external assignments.
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;

    // Check the memory of the body of the closure.
    sm->MoveToNextScope();
    Body->Stage8_CheckMemory(sm, meta);

    // Set the scope back.
    meta->Restore();
    sm->CurrentScope = parent_scope;
}

auto spp::asts::ClosureExpressionAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Strategy: build an "environment" struct for the closure. Attributes are captures. The safety is already
    // guaranteed by semantic analysis.
    // Todo: Add LLVM attributes to pointer types for optimizations (unique, nonnull, etc).
    const auto uid = spp::utils::Uid(this);
    const auto env_ty = llvm::StructType::create(*ctx->Context, "$ClosureEnv" + uid);
    auto env_field_types = Vec<llvm::Type*>{};
    for (auto const &capture : PcGroup->CaptureGroup->Captures) {
        const auto cap_ty = capture->InferType(sm, meta);
        const auto cap_ty_sym = sm->CurrentScope->GetTypeSymbol(cap_ty);
        env_field_types.EmplaceBack(codegen::llvm_type(*cap_ty_sym, ctx));
    }
    env_ty->setBody(env_field_types.ToStdVector(), false);

    // Build a new function that the body of the closure is built into. Needs a variable binding at the top (ie allow
    // "let a = env.a" as the function signature will be "(env: $ClosureX, ...params: Params) -> RetType").
    auto llvm_param_types = PcGroup->ParamGroup->GetAllParams()
        | std::views::transform([&](auto const &param) { return codegen::llvm_type(*sm->CurrentScope->GetTypeSymbol(param->Type.Get()), ctx); })
        | std::ranges::to<Vec>();
    llvm_param_types.Insert(llvm_param_types.begin(), llvm::PointerType::get(*ctx->Context, 0));
    const auto llvm_ret_ty = codegen::llvm_type(*sm->CurrentScope->GetTypeSymbol(_RetType.Get()), ctx);
    const auto llvm_fn_ty = llvm::FunctionType::get(llvm_ret_ty, llvm_param_types.ToStdVector(), PcGroup->ParamGroup->GetVariadicParams() != nullptr);
    const auto llvm_fn = llvm::Function::Create(llvm_fn_ty, llvm::Function::InternalLinkage, "$closure_fn_" + uid, ctx->Module.Get());
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry", llvm_fn);

    const auto saved_bb = ctx->Builder.GetInsertBlock();
    const auto saved_fn_scope = meta->EnclosingFunctionScope;
    auto saved_ret_ty = std::move(meta->EnclosingFunctionRetType);
    const auto saved_src_ret_ty = meta->EnclosingFunctionSourceRetType;
    const auto saved_flavour = meta->EnclosingFunctionFlavour;
    const auto saved_current_closure_type = ctx->CurrentClosureType;

    ctx->Builder.SetInsertPoint(entry_bb);
    sm->CurrentScope->AstNode = this;
    _LlvmFunc = MakeUnique<codegen::LlvmFuncWrapper>(llvm_fn);
    meta->EnclosingFunctionScope = sm->CurrentScope;
    meta->EnclosingFunctionRetType.Clear();
    meta->EnclosingFunctionRetType.EmplaceBack(AstClone(_RetType));
    meta->EnclosingFunctionSourceRetType = {Source._OriginalRetType.Get()};
    meta->EnclosingFunctionFlavour = Tok.Get();
    ctx->CurrentClosureType = env_ty;
    ctx->CurrentClosureScope = sm->CurrentScope;

    // For now, just skip scopes and return a nullptr.
    const auto parent_scope = sm->CurrentScope;
    meta->Save();
    PcGroup->Stage11_CodeGen(sm, meta, ctx);
    sm->MoveToNextScope();
    Body->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    sm->CurrentScope = parent_scope;

    // Restore the previous context.
    ctx->Builder.SetInsertPoint(saved_bb);
    meta->EnclosingFunctionScope = saved_fn_scope;
    meta->EnclosingFunctionRetType = std::move(saved_ret_ty);
    meta->EnclosingFunctionSourceRetType = saved_src_ret_ty;
    meta->EnclosingFunctionFlavour = saved_flavour;
    ctx->CurrentClosureType = saved_current_closure_type;

    // Allocate the closure environment.
    const auto env_alloca = ctx->Builder.CreateAlloca(env_ty, nullptr, "closure.env.alloca." + uid);
    for (auto const &[i, capture] : PcGroup->CaptureGroup->Captures | std::views::enumerate) {
        const auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 0);
        const auto capture_index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), static_cast<std::uint64_t>(i));

        const auto field_ptr = ctx->Builder.CreateInBoundsGEP(
            env_ty, env_alloca, {zero, capture_index}, "closure.env.gep." + std::to_string(i));
        const auto val = capture->Val->Stage11_CodeGen(sm, meta, ctx); // always an identifier, no worry abt double generation.
        ctx->Builder.CreateStore(val, field_ptr);
    }

    // Allocate the closure object.
    const auto closure_ty = llvm::StructType::create(*ctx->Context, "$ClosureObj" + uid);
    const auto closure_alloca = ctx->Builder.CreateAlloca(closure_ty, nullptr, "closure.obj.alloca." + uid);
    ctx->Builder.CreateStore(
        llvm_fn,
        ctx->Builder.CreateInBoundsGEP(closure_ty, closure_alloca, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 0)}));

    ctx->Builder.CreateStore(
        env_alloca,
        ctx->Builder.CreateInBoundsGEP(closure_ty, closure_alloca, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 1)}));

    // Return the generated closure.
    return ctx->Builder.CreateLoad(closure_ty, closure_alloca, "load.closure.obj." + uid);
}

auto spp::asts::ClosureExpressionAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Create the type as a nullptr, so it can be analysed later.
    using generate::common_types::FunRefType;
    using generate::common_types::FunMutType;
    using generate::common_types::FunMovType;
    using generate::common_types::TupleType;
    Unique<TypeAst> ty = nullptr;

    auto is_ref_cap = [](auto const &cap) { return cap->Conv != nullptr and *cap->Conv == ConventionTag::REF; };
    auto is_mut_cap = [](auto const &cap) { return cap->Conv != nullptr and *cap->Conv == ConventionTag::MUT; };

    // If there are no captures, return a FunRef type with the parameters and return type.
    if (PcGroup->CaptureGroup->Captures.IsEmpty()) {
        auto param_types = PcGroup->ParamGroup->Params
            | std::views::transform([](auto const &x) { return asts::AstClone(x->Type); })
            | std::ranges::to<Vec>();
        ty = FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), AstClone(_RetType));
    }

    else if (std::ranges::any_of(PcGroup->CaptureGroup->Captures, [](auto const &x) { return x->Conv == nullptr; })) {
        // If there are captures, but no borrowed captures, return a FunMov type with the parameters and return type.
        auto param_types = PcGroup->ParamGroup->Params
            | std::views::transform([](auto const &x) { return asts::AstClone(x->Type); })
            | std::ranges::to<Vec>();
        ty = FunMovType(PosStart(), TupleType(PosStart(), std::move(param_types)), AstClone(_RetType));
    }

    else if (std::ranges::any_of(PcGroup->CaptureGroup->Captures, is_mut_cap)) {
        // If there are mutably borrowed captures, return a FunMut type with the parameters and return type.
        auto param_types = PcGroup->ParamGroup->Params
            | std::views::transform([](auto const &x) { return asts::AstClone(x->Type); })
            | std::ranges::to<Vec>();
        ty = FunMutType(PosStart(), TupleType(PosStart(), std::move(param_types)), AstClone(_RetType));
    }

    else if (std::ranges::any_of(PcGroup->CaptureGroup->Captures, is_ref_cap)) {
        // If there are immutable borrowed captures, return a FunRef type with the parameters and return type.
        auto param_types = PcGroup->ParamGroup->Params
            | std::views::transform([](auto const &x) { return asts::AstClone(x->Type); })
            | std::ranges::to<Vec>();
        ty = FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), AstClone(_RetType));
    }

    // Analyse the type and return it.
    ty->Stage7_AnalyseSemantics(sm, meta);
    CACHE_TYPE_INFERENCE_AND_RETURN(ty);
}

auto spp::asts::ClosureExpressionAst::GetLlvmFunc() const
    -> codegen::LlvmFuncWrapper* {
    return _LlvmFunc.Get();
}

SPP_MOD_END
