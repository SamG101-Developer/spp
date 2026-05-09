module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

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
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
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
    ast->LlvmCoroYieldSlot = LlvmCoroYieldSlot;
    ast->LlvmGenEnv = LlvmGenEnv;
    ast->_LlvmResumeFunc = _LlvmResumeFunc;
    for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
    return ast;
}

auto spp::asts::CoroutinePrototypeAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::type_utils::IsTypeGen;
    using analyse::errors::SppCoroutineInvalidReturnTypeError;

    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::Stage7_AnalyseSemantics(sm, meta);
    const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType);

    // Update the meta information for enclosing function information.
    meta->Save();
    meta->EnclosingFunctionFlavour = TokFun.get();
    meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFunctionScope = sm->CurrentScope;
    Impl->Stage7_AnalyseSemantics(sm, meta);

    // Check the return type superimposes the generator type.
    auto temp = Vec<Shared<TypeAst>>();
    temp.EmplaceBack(ret_type_sym->FqName()->WithoutGenerics());
    auto superimposed_types = ret_type_sym->LinkedScope->SupTypes()
        | genex::views::concat(std::move(temp))
        | genex::views::transform([](auto const &x) { return x->WithoutGenerics(); })
        | genex::to<Vec>();

    RaiseIf<SppCoroutineInvalidReturnTypeError>(
        genex::none_of(superimposed_types, [sm](auto const &x) { return IsTypeGen(*x->WithoutGenerics(), *sm->CurrentScope); }),
        {sm->CurrentScope}, ERR_ARGS(*this, *Source.OriginalReturnType, *ReturnType));

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

    // Create the coroutine constructor function.
    const auto [llvm_coro_ctor, llvm_gen_env, llvm_gen_env_args_type] = codegen::create_coro_gen_ctor(this, ctx, *sm->CurrentScope);
    if (llvm_gen_env != nullptr) {
        const auto uid = spp::utils::Uid(this);
        const auto llvm_coro_resume_func = codegen::create_coro_res_func(this, llvm_gen_env_args_type, ctx, *sm->CurrentScope);
        _LlvmResumeFunc = llvm_coro_resume_func; // Save for interaction with ".res()" calls.
        LlvmGenEnv = llvm_gen_env; // Save for interaction with ".res()" calls.

        // Load the resume function into the 0th field of the coroutine environment.
        ctx->Builder.CreateStore(
            ctx->Builder.CreateBitCast(llvm_coro_resume_func, llvm::PointerType::get(*ctx->Context, 0)),
            ctx->Builder.CreateStructGEP(llvm_coro_ctor->getReturnType(), llvm_gen_env, static_cast<std::uint8_t>(codegen::GenEnvField::RES_FN)));

        // Entry block into the resume function.
        const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry" + uid, llvm_coro_resume_func);
        ctx->Builder.SetInsertPoint(entry_bb);

        const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType);
        meta->Save();
        meta->EnclosingFunctionFlavour = TokFun.get();
        meta->EnclosingFunctionScope = sm->CurrentScope;
        meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
        Impl->Stage11_CodeGen(sm, meta, ctx);
        meta->Restore();

        // Reset to the start of the resume function to build the switch.
        ctx->Builder.SetInsertPoint(entry_bb);

        // Create the "switch" header block, mapping location values to labels.
        const auto number_of_yields = static_cast<std::uint32_t>(ctx->YieldContinuations.Len());
        const auto switch_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.switch" + uid, llvm_coro_resume_func);

        // Switch on the value loaded from the coroutine environment's location field.
        const auto loc_field = ctx->Builder.CreateStructGEP(llvm::PointerType::get(*ctx->Context, 0), llvm_coro_resume_func->getArg(0), 1, "coro.loc.gep" + uid);
        const auto loc_value = ctx->Builder.CreateLoad(llvm::Type::getInt32Ty(*ctx->Context), loc_field, "coro.loc.load" + uid);
        const auto switch_inst = ctx->Builder.CreateSwitch(loc_value, switch_bb, number_of_yields + 1);

        // Case for "0" => start of the coroutine.
        const auto start_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.start" + uid, llvm_coro_resume_func);
        switch_inst->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 0), start_bb);
        ctx->Builder.SetInsertPoint(start_bb);

        // Loop up the yield counter, generating a "case" and "jump" for each yield point.
        for (std::size_t i = 0; i < number_of_yields; ++i) {
            const auto target_block = ctx->YieldContinuations[i];
            switch_inst->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), i + 1), target_block);
        }

        // Reset the yield continuations for future coroutines.
        ctx->YieldContinuations.Clear();
    }
    else {
        // Generic base function so not generating for it.
        // Manual scope skipping.
        const auto final_scope = sm->CurrentScope->FinalChildScope();
        while (sm->CurrentScope != final_scope) {
            sm->MoveToNextScope(false);
        }
    }
    sm->MoveOutOfCurrentScope();

    // Can't use "llvm_func == nullptr" because of coroutine ctor function.
    if (llvm_gen_env == nullptr) {
        // Analyse to make a new scope in the correct place.
        for (auto const &[_, generic_impl] : _GenericSubstitutions) {
            auto tm = ScopeManager(sm->GlobalScope, _Scope->Parent);
            tm.Reset(tm.CurrentScope);
            generic_impl->Stage11_CodeGen(&tm, meta, ctx);
        }
    }

    return llvm_coro_ctor;
}

SPP_MOD_END
