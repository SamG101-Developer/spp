module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_function_call_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.overload_utils;
import spp.analyse.utils.type_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.fold_expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
    decltype(GnArgGroup) &&generic_arg_group,
    decltype(FnArgGroup) &&arg_group,
    decltype(Fold) &&fold) :
    GnArgGroup(std::move(generic_arg_group)),
    FnArgGroup(std::move(arg_group)),
    Fold(std::move(fold)),
    _OverloadInfo(std::nullopt),
    _ClosureDummyArg(nullptr),
    _ClosureDummyProto(nullptr),
    _IsAsync(nullptr),
    _IsCoroAndAutoResume(false) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
    Source.OriginalExpr = this;
}

spp::asts::PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::PosStart() const
    -> std::size_t {
    // Use the generic argument group.
    return not GnArgGroup->Args.IsEmpty() ? GnArgGroup->PosStart() : FnArgGroup->PosStart();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::PosEnd() const
    -> std::size_t {
    // Use the fold or function argument group.
    return Fold ? Fold->PosEnd() : FnArgGroup->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
        AstClone(GnArgGroup), AstClone(FnArgGroup), AstClone(Fold));
    if (Source.OriginalExpr != this) {
        ast->Source.OriginalExpr = Source.OriginalExpr;
    }
    ast->_ClosureDummyProto = AstClone(_ClosureDummyProto);
    ast->_TransformedAst = AstClone(_TransformedAst);
    ast->_OverloadInfo = _OverloadInfo;
    ast->_IsAsync = _IsAsync;
    ast->_FoldedAsts = AstCloneVec(_FoldedAsts);
    ast->_ClosureDummyArgGroup = AstClone(_ClosureDummyArgGroup);
    ast->_ClosureDummyArg = AstClone(_ClosureDummyArg);
    ast->_IsCoroAndAutoResume = _IsCoroAndAutoResume;
    return ast;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(GnArgGroup);
    SPP_STRING_APPEND(FnArgGroup);
    SPP_STRING_APPEND(Fold);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidComptimeOperationError;
    using analyse::utils::func_utils::IsTargetCallable;
    using analyse::utils::overload_utils::DetermineOverload;
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types_precompiled::FUN_REF;
    using generate::common_types_precompiled::FUN_MUT;
    using generate::common_types_precompiled::GEN_ONCE;

    // Prevent double analysis.
    // Todo: See why this might be happening anyway, and remove this check preferably.
    if (_OverloadInfo.has_value()) { return; }

    // Analyse the generic arguments and the function call arguments before determining the overload.
    meta->Save();
    meta->ReturnTypeOverloadResolverType = nullptr;
    GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
    FnArgGroup->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();

    // If we are function folding, create transformed asts.
    if (Fold != nullptr) {
        _FoldedAsts = _HandleFunctionFolding(sm, meta);
        for (auto const &ast : _FoldedAsts) { ast->Stage7_AnalyseSemantics(sm, meta); }
        return;
    }

    // Resolve the overload for this function call.
    auto [overload, is_closure] = DetermineOverload(*this, sm, meta);

    // Special case for closures; apply the convention the closure name to ensure is it movable/mutable etc.
    if (is_closure) {
        const auto lhs_type = IsTargetCallable(*meta->PostfixExpressionLhs, *sm, meta);
        auto dummy_self_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
            nullptr, nullptr, AstClone(meta->PostfixExpressionLhs));

        if (TypeEq(*lhs_type->WithoutGenerics(), *FUN_MUT, *sm->CurrentScope, *sm->CurrentScope)) {
            dummy_self_arg->Conv = MakeUnique<ConventionMutAst>(nullptr, nullptr);
            dummy_self_arg->Conv->To<ConventionMutAst>()->TokBorrow->PatchPos(meta->PostfixExpressionLhs->PosStart());
        }
        else if (TypeEq(*lhs_type->WithoutGenerics(), *FUN_REF, *sm->CurrentScope, *sm->CurrentScope)) {
            dummy_self_arg->Conv = MakeUnique<ConventionRefAst>(nullptr);
            dummy_self_arg->Conv->To<ConventionRefAst>()->TokBorrow->PatchPos(meta->PostfixExpressionLhs->PosStart());
        }
        _ClosureDummyArg = std::move(dummy_self_arg);
    }

    // Set the overload to the only pass overload.
    _OverloadInfo = _OInfo{
        .OverloadScope = std::get<0>(overload),
        .Proto = std::get<1>(overload),
        .GnArgs = std::move(std::get<3>(overload))
    };
    if (const auto self_param = _OverloadInfo->Proto->FnParamGroup->GetSelfParam()) {
        FnArgGroup->Args[0]->Conv = AstClone(self_param->Conv);
    }
    FnArgGroup->Args = std::move(std::get<2>(overload)->Args);

    // Check that if we are in a cmp context, that the overload is also cmp.
    RaiseIf<SppInvalidComptimeOperationError>(
        meta->EnclosingFunctionCmp != nullptr and _OverloadInfo->Proto->TokCmp == nullptr,
        {sm->CurrentScope}, ERR_ARGS(*this));

    // Special case for GenOnce called as a coroutine => auto move into the "Yield" type.
    if (_OverloadInfo->Proto->TokFun->TokenType == lex::SppTokenType::KW_COR and not meta->PreventAutoGeneratorResume) {
        // This needs to be any type that EXTENDS GenOnce, not just GenOnce itself.
        auto [_, _, is_once] = analyse::utils::type_utils::GetGenAndYieldTypes(
            *_OverloadInfo->Proto->ReturnType, *sm->CurrentScope, *meta->PostfixExpressionLhs, "GenOnce collapse");
        _IsCoroAndAutoResume = is_once;
    }
    meta->PreventAutoGeneratorResume = false;

    // Copy some properties into the transform (clone arg group for the self arg convention).
    if (_TransformedAst) {
        const auto transformed_op = _TransformedAst->Op->To<PostfixExpressionOperatorFunctionCallAst>();
        transformed_op->FnArgGroup = AstClone(FnArgGroup);
        transformed_op->_OverloadInfo = _OverloadInfo;
        transformed_op->_IsAsync = _IsAsync;
        transformed_op->_FoldedAsts = AstCloneVec(_FoldedAsts);
        transformed_op->_ClosureDummyArg = AstClone(_ClosureDummyArg);
    }
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If a fold is taking place, analyse the folded transformations.
    if (Fold != nullptr) {
        for (auto const &ast : _FoldedAsts) { ast->Stage8_CheckMemory(sm, meta); }
        return;
    }

    // If a closure is being called, apply memory rules to the symbolic target.
    if (_ClosureDummyArg != nullptr) {
        auto closure_args = Vec<Unique<FunctionCallArgumentAst>>();
        closure_args.EmplaceBack(std::move(_ClosureDummyArg));
        _ClosureDummyArgGroup = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, std::move(closure_args), nullptr);
        _ClosureDummyArgGroup->Stage7_AnalyseSemantics(sm, meta);
        _ClosureDummyArgGroup->Stage8_CheckMemory(sm, meta);
    }

    // Check the argument group, now the old borrows hae been invalidated.
    GnArgGroup->Stage8_CheckMemory(sm, meta);

    meta->Save();
    meta->TargetCallFunctionPrototype = _OverloadInfo->Proto;
    meta->TargetCallWasFunctionAsync = _IsAsync;
    FnArgGroup->Stage8_CheckMemory(sm, meta);
    meta->Restore();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppCompileTimeConstantError;
    using analyse::errors::SppCompileTimeConstantError;

    // Get the function prototype and resolve it.
    const auto fn_proto = _OverloadInfo->Proto->GetNonGenericImpl();
    RaiseIf<SppCompileTimeConstantError>(
        fn_proto->TokCmp == nullptr,
        {sm->CurrentScope}, ERR_ARGS(*this));

    // Todo: For now, don't allow folding in comptime.
    RaiseIf<SppCompileTimeConstantError>(
        Fold != nullptr,
        {sm->CurrentScope}, ERR_ARGS(*Fold));

    // Create the argument map for the function to use. Positional arguments (including the implicit "self"
    // injected for method-call syntax) are matched to parameters by position; keyword arguments by name.
    const auto fn_params = fn_proto->FnParamGroup->GetAllParams();
    auto args = Vec<Pair<Shared<IdentifierAst>, Unique<ExpressionAst>>>();
    for (auto const &[i, arg] : FnArgGroup->GetAllArgs() | genex::views::enumerate) {
        const auto kw_arg = arg->To<FunctionCallArgumentKeywordAst>();
        auto name = kw_arg != nullptr ? kw_arg->Name : fn_params[i]->ExtractName();
        arg->Stage9_CompTimeResolve(sm, meta);
        args.EmplaceBack(std::move(name), std::move(meta->CmpResult));
    }
    auto arg_map = decltype(meta->CmpArgs)();
    for (auto &&[name, val] : args) { arg_map[name] = std::move(val); }

    // Resolve the function with the arguments.
    meta->Save();
    meta->CmpArgs = std::move(arg_map);
    auto tm = ScopeManager(sm->GlobalScope, fn_proto->GetAstScope()); // const_cast<analyse::scopes::Scope*>(std::get<0>(*m_overload_info)));
    tm.Reset(tm.CurrentScope->Children[0].get());
    fn_proto->Impl->Stage9_CompTimeResolve(&tm, meta);
    meta->Restore();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // For folding, generate the code for the folded transformations and combine into single block.
    if (Fold != nullptr) {
        const auto merge = InnerScopeExpressionAst::NewEmpty();
        merge->Members = _FoldedAsts
            | genex::views::transform([&meta](auto &&ast) {
                auto clone_lhs = AstClone(meta->PostfixExpressionLhs);
                auto pf = MakeUnique<PostfixExpressionAst>(std::move(clone_lhs), std::move(ast));
                return Unique<StatementAst>(pf.release());
            })
            | genex::to<Vec>();
        return merge->Stage11_CodeGen(sm, meta, ctx);
    }

    // For generically converted function prototypes, generate their llvm func once.
    // Todo: Is this even needed?
    if (_OverloadInfo->Proto->GetLlvmFunc() == nullptr) {
        auto tm = ScopeManager(sm->GlobalScope, const_cast<analyse::scopes::Scope*>(_OverloadInfo->OverloadScope));
        tm.Reset(tm.CurrentScope);
        _OverloadInfo->Proto->Stage10_PreCodeGen(&tm, meta, ctx);
    }

    // SPP_ASSERT(not ctx->Builder.GetInsertBlock()->getTerminator());
    const auto uid = spp::utils::Uid(this);
    auto llvm_self_arg = static_cast<llvm::Value*>(nullptr);

    if (not FnArgGroup->GetKeywordArgs().IsEmpty() and FnArgGroup->GetKeywordArgs()[0]->Name->Val == "self") {
        // Get the type of the left-hand-side expression.
        const auto lhs_type = meta->PostfixExpressionLhs->To<PostfixExpressionAst>()->Lhs->InferType(sm, meta);
        const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);
        const auto llvm_type = codegen::llvm_type(*lhs_type_sym, ctx);
        SPP_ASSERT(llvm_type != nullptr);

        // If the lhs is non-symbolic, we need to materialize it, and use as the self argument.
        const auto temp_lhs = meta->PostfixExpressionLhs->To<PostfixExpressionAst>()->Lhs.get();
        const auto [var_sym, _] = sm->CurrentScope->GetVarSymbolOutermost(*temp_lhs);
        auto base_ptr = static_cast<llvm::Value*>(nullptr);
        if (var_sym != nullptr) {
            // Get the alloca for the lhs symbol (the base pointer).
            const auto lhs_alloca = var_sym->LlvmInfo->Alloca;
            SPP_ASSERT(lhs_alloca != nullptr);
            base_ptr = ctx->Builder.CreateLoad(llvm_type, lhs_alloca, "load.member_access.base_ptr" + uid);
        }

        // This check prevents materializing static access, like "Vec::Vec::push" as it makes no sense to do so.
        else if (sm->CurrentScope->GetTypeSymbol(AstClone(temp_lhs->To<TypeAst>())) == nullptr) {
            // Materialize the lhs expression into a temporary.
            const auto lhs_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
            const auto temp = ctx->Builder.CreateAlloca(llvm_type, nullptr, "temp.member_access.lhs" + uid);
            ctx->Builder.CreateStore(lhs_val, temp);
            base_ptr = temp;
        }

        // Use the proper pointer to the self arg (sometimes for semantic analysis dummy vars are used).
        llvm_self_arg = ctx->Builder.CreateLoad(llvm_type, base_ptr, "load.member_access.self_arg" + uid);
    }

    // Get the llvm function target, and generate the argument values.
    const auto llvm_func = _OverloadInfo->Proto->GetLlvmFunc()->Target;
    SPP_ASSERT(llvm_func != nullptr);
    auto llvm_func_args = FnArgGroup->Args
        | genex::views::transform([sm, meta, ctx](auto const &x) { return x->Stage11_CodeGen(sm, meta, ctx); })
        | genex::to<Vec>();
    if (llvm_self_arg != nullptr) { llvm_func_args[0] = llvm_self_arg; }

    // Create the call instruction.
    const auto llvm_call = ctx->Builder.CreateCall(
        llvm_func, llvm_func_args.ToStdVector(), "call" + uid);
    return llvm_call;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    //
    using generate::common_types::SelfType;
    using generate::common_types::TupleType;
    using analyse::utils::type_utils::ResolveAndSubstituteSelfType;

    // For function folding, collect a tuple of all return types.
    if (not _FoldedAsts.IsEmpty()) {
        auto folded_return_types = _FoldedAsts
            | genex::views::transform([sm, meta](auto const &ast) { return ast->InferType(sm, meta); })
            | genex::to<Vec>();
        auto tuple_type = TupleType(0, std::move(folded_return_types));
        return tuple_type;
    }

    // Get the function return type from the overload.
    auto ret_type = _OverloadInfo->Proto->ReturnType;

    // If there is a scope present (non-closure), then fully qualify the return type.
    if (_OverloadInfo->OverloadScope != nullptr and not ret_type->IsSelfType()) {
        ret_type = _OverloadInfo->OverloadScope->GetTypeSymbol(ret_type)->FqName();
    }

    // For GenOnce coroutines, automatically resume the coroutine and return the "Yield" type.
    if (_IsCoroAndAutoResume and not meta->PreventAutoGeneratorResume) {
        auto [_, yield_type, _] = analyse::utils::type_utils::GetGenAndYieldTypes(
            *ret_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "function call");
        ret_type = yield_type;
    }

    const auto pf = meta->PostfixExpressionLhs->To<PostfixExpressionAst>();
    const auto is_runtime = pf ? pf->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr : false;
    const auto is_static = pf ? pf->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>() != nullptr and pf->Lhs->To<TypeAst>() : false;

    if (ret_type->IsSelfType()) {
        ret_type = meta->PostfixExpressionLhs->To<PostfixExpressionAst>()->Lhs->InferType(sm, meta)->WithConvention(nullptr);
    }
    else if (pf and (is_runtime or is_static)) {
        // Perform a "Self=FQType" substitution to handle "Self" being part of the generics of the return type.
        // Todo: use Resolve method (which scope??)
        const auto inferred = is_runtime
            ? pf->Lhs->InferType(sm, meta)
            : AstClone(pf->Lhs->To<TypeAst>());

        auto generic = MakeUnique<GenericArgumentTypeKeywordAst>(
            SelfType(0), nullptr,
            inferred->WithConvention(nullptr));

        const auto generic_group = GenericArgumentGroupAst::NewEmpty();
        generic_group->Args.PushBack(std::move(generic));
        ret_type = ret_type->SubstituteGenerics(generic_group->GetAllArgs());
    }

    // Return the type.
    return ret_type;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::MarkAsAsync(
    Ast *async_token)
    -> void {
    _IsAsync = async_token;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Target() const
    -> FunctionPrototypeAst* {
    return _OverloadInfo.has_value() ? _OverloadInfo->Proto : nullptr;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::SetClosureDummyProto(
    Unique<FunctionPrototypeAst> &&proto) -> void {
    _ClosureDummyProto = std::move(proto);
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::SetTransformedAst(
    Unique<PostfixExpressionAst> &&ast)
    -> void {
    _TransformedAst = std::move(ast);
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::GetTransformedAst() const
    -> PostfixExpressionAst* {
    return _TransformedAst.get();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::_HandleFunctionFolding(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> {
    // Populate the list of arguments to fold.
    auto folded_args = Vec<FunctionCallArgumentAst*>{};
    auto folded_arg_types = Vec<TypeAst*>{};
    auto folded_tup_lens = Vec<std::size_t>{};
    auto fold_indexes = Vec<std::size_t>{};
    for (auto [i, arg] : FnArgGroup->GetAllArgs() | genex::views::enumerate) {
        auto arg_type = arg->InferType(sm, meta);
        if (analyse::utils::type_utils::IsTypeTup(*arg_type, *sm->CurrentScope)) {
            fold_indexes.EmplaceBack(i);
            folded_args.EmplaceBack(arg);
            folded_arg_types.EmplaceBack(arg_type.get());
            folded_tup_lens.EmplaceBack(arg_type->TypeParts().Back()->GnArgGroup->Args.Len());
        }
    }

    // Build the unrolled AST transformations.
    const auto smallest_tuple = genex::min_element(folded_tup_lens);
    auto transformed_asts = Vec<Unique<PostfixExpressionOperatorFunctionCallAst>>{};
    for (auto i = 0uz; i < smallest_tuple; ++i) {
        auto new_arg_group = AstClone(FnArgGroup);
        for (const auto fold_index : fold_indexes) {
            // Create the postfix access into the tuple.
            auto id = MakeUnique<IdentifierAst>(new_arg_group->Args[fold_index]->Val->PosEnd(), std::to_string(i));
            auto ma = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(id));
            auto pf = MakeUnique<PostfixExpressionAst>(std::move(new_arg_group->Args[fold_index]->Val), std::move(ma));
            new_arg_group->Args[fold_index]->Val = std::move(pf);
        }

        auto transformed_ast = AstClone(this);
        transformed_ast->FnArgGroup = std::move(new_arg_group);
        transformed_ast->Fold = nullptr;
        transformed_asts.EmplaceBack(std::move(transformed_ast));
    }

    // Return the transformed asts.
    return transformed_asts;
}

SPP_MOD_END
