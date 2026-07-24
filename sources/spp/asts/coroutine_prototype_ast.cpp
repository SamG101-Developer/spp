module;
#include <spp/macros.hpp>

module spp.asts.coroutine_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.identifier_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
spp::asts::CoroutinePrototypeAst::CoroutinePrototypeAst(
    decltype(Annotations) &&annotations,
    decltype(TokCmp) &&tok_cmp,
    decltype(TokFun) &&tok_fun,
    decltype(Name) &&name,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(FnParamGroup) &&param_group,
    decltype(TokArrow) &&tok_arrow,
    decltype(ReturnType) &&return_type,
    decltype(Impl) &&impl) :
    FunctionPrototypeAst(
        std::move(annotations), std::move(tok_cmp), std::move(tok_fun), std::move(name),
        std::move(generic_param_group), std::move(param_group), std::move(tok_arrow),
        std::move(return_type), std::move(impl)),
    LlvmCoroGenEnv(nullptr),
    LlvmCoroGenEnvType(nullptr),
    LlvmCoroResumeFunc(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokFun, lex::SppTokenType::KW_COR, "cor");
}

spp::asts::CoroutinePrototypeAst::~CoroutinePrototypeAst() = default;

auto spp::asts::CoroutinePrototypeAst::Clone() const
    -> Unique<Ast> {
    auto ast = MakeUnique<CoroutinePrototypeAst>( // Todo: why no "cmp"?
        AstCloneVec(Annotations), nullptr, AstClone(TokFun), AstClone(Name), AstClone(GnParamGroup),
        AstClone(FnParamGroup), AstClone(TokArrow), AstClone(ReturnType), AstClone(Impl));
    ast->_AnnotationInfo = _AnnotationInfo
        ? MakeUnique<analyse::utils::annotation_utils::AnnotationInfo>(*_AnnotationInfo)
        : nullptr;
    ast->Source.OriginalImpl = AstClone(Source.OriginalImpl);
    ast->Source.OriginalReturnType = AstClone(Source.OriginalReturnType);
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    ast->AbstractAnnotation = AbstractAnnotation;
    ast->VirtualAnnotation = VirtualAnnotation;
    ast->TemperatureAnnotation = TemperatureAnnotation;
    ast->FfiAnnotation = FfiAnnotation;
    ast->BuiltinAnnotation = BuiltinAnnotation;
    ast->InlineAnnotation = InlineAnnotation;
    ast->Visibility = Visibility;
    ast->_LlvmFunc = _LlvmFunc;
    ast->LlvmCoroGenEnv = LlvmCoroGenEnv;
    ast->LlvmCoroResumeFunc = LlvmCoroResumeFunc;
    for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
    return ast;
}

auto spp::asts::CoroutinePrototypeAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::type_utils::IsTypeGen;
    using analyse::utils::type_utils::GetGenAndYieldTypes;

    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::Stage7_AnalyseSemantics(sm, meta);
    const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType.get());

    // Update the meta information for enclosing function information.
    meta->Save();
    meta->EnclosingFunctionFlavour = TokFun.get();
    meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
    meta->EnclosingFunctionScope = sm->CurrentScope;
    Impl->Stage7_AnalyseSemantics(sm, meta);

    // Check the return type superimposes the generator type.
    GetGenAndYieldTypes(
        *ret_type_sym->FqName(), *sm->CurrentScope, *Source.OriginalReturnType, "coroutine return type");

    // Analyse the semantics of the function body, and move out the scope.
    sm->MoveOutOfCurrentScope();
    meta->Restore(true);
    meta->LoopReturnTypes->clear();
}

auto spp::asts::CoroutinePrototypeAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move into the coroutine scope.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);
    const auto uid = spp::utils::Uid(this);
    const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

    // Build the coroutine's env struct (its frame). A null result means this is a generic base (its types are not
    // concrete), so no code is generated for it.
    const auto env_type = LlvmCoroGenEnvType != nullptr
        ? LlvmCoroGenEnvType
        : codegen::CreateCoroEnvType(this, ctx, *sm->CurrentScope);
    auto result = static_cast<llvm::Value*>(nullptr);

    if (env_type != nullptr) {
        // Create the resume function "(env*, send) -> void" if not already made. Its prologue binds the env pointer
        // (LlvmGenEnv) and points every frame variable at its env field.
        const auto resume_func = LlvmCoroResumeFunc != nullptr
            ? LlvmCoroResumeFunc
            : codegen::CreateCoroResFunc(this, ctx, *sm->CurrentScope);
        const auto env_ptr = resume_func->getArg(0);
        ctx->Builder.SetInsertPoint(&resume_func->getEntryBlock());
        result = GetLlvmFunc()->Target;

        // Switch on the location field: 0 => start of the body, i+1 => the i-th "gen" continuation.
        const auto loc_slot = ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(codegen::GenEnvField::LOCATION), "coro.loc.slot" + uid);
        const auto loc_val = ctx->Builder.CreateLoad(i32_ty, loc_slot, "coro.loc.load" + uid);
        const auto bad_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.bad" + uid, resume_func);
        const auto start_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.start" + uid, resume_func);
        const auto switch_inst = ctx->Builder.CreateSwitch(loc_val, bad_bb);
        switch_inst->addCase(llvm::ConstantInt::get(i32_ty, 0), start_bb);

        ctx->Builder.SetInsertPoint(bad_bb);
        ctx->Builder.CreateUnreachable();

        // Generate the coroutine body into the start block. The "gen" expressions register continuation blocks.
        ctx->Builder.SetInsertPoint(start_bb);
        const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType.get());
        meta->Save();
        meta->EnclosingFunctionFlavour = TokFun.get();
        meta->EnclosingFunctionScope = sm->CurrentScope;
        meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
        meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
        Impl->Stage11_CodeGen(sm, meta, ctx);
        meta->Restore();

        // The body falling through means the coroutine is exhausted: mark EXHAUSTED and return.
        if (ctx->Builder.GetInsertBlock()->getTerminator() == nullptr) {
            ctx->Builder.CreateStore(
                llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), std::to_underlying(codegen::CoroutineState::EXHAUSTED)),
                ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(codegen::GenEnvField::STATE), "coro.state.done" + uid));
            ctx->Builder.CreateRetVoid();
        }

        // Wire each "gen" continuation into the switch (location i+1 => the i-th continuation block).
        for (auto const &[i, cont_bb] : ctx->YieldContinuations | genex::views::enumerate) {
            switch_inst->addCase(llvm::ConstantInt::get(i32_ty, i + 1), cont_bb);
        }
        ctx->YieldContinuations.Clear();

        // The coroutine's own function is never called directly. Emit an empty stub so it remains a valid (defined)
        // internal function.
        const auto main_func = GetLlvmFunc()->Target;
        if (main_func->empty()) {
            const auto stub_bb = llvm::BasicBlock::Create(*ctx->Context, "entry" + uid, main_func);
            ctx->Builder.SetInsertPoint(stub_bb);
            if (main_func->getReturnType()->isVoidTy()) { ctx->Builder.CreateRetVoid(); }
            else { ctx->Builder.CreateRet(llvm::UndefValue::get(main_func->getReturnType())); }
        }
    }
    else {
        // Generic base function so not generating for it. Manual scope skipping.
        const auto final_scope = sm->CurrentScope->FinalChildScope();
        while (sm->CurrentScope != final_scope) { sm->MoveToNextScope(false); }
    }
    sm->MoveOutOfCurrentScope();

    // For a generic base, generate each of its concrete substitutions.
    if (env_type == nullptr) {
        for (auto const &[_, generic_impl] : _GenericSubstitutions) {
            auto tm = ScopeManager(sm->GlobalScope, _Scope->Parent);
            tm.Reset(tm.CurrentScope);
            generic_impl->Stage11_CodeGen(&tm, meta, ctx);
        }
    }

    return result;
}

auto spp::asts::CoroutinePrototypeAst::IsCoroutine() const
    -> bool {
    return true;
}

SPP_MOD_END
