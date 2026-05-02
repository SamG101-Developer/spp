module;
#include <spp/macros.hpp>

module spp.asts.function_implementation_lowered_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.builtins;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.traits;
import genex;

SPP_MOD_BEGIN
auto spp::asts::FunctionImplementationLoweredAst::NewEmpty()
    -> Unique<FunctionImplementationLoweredAst> {
    // Empty AST.
    return MakeUnique<FunctionImplementationLoweredAst>(nullptr, decltype(Members)(), nullptr);
}

spp::asts::FunctionImplementationLoweredAst::~FunctionImplementationLoweredAst() = default;

auto spp::asts::FunctionImplementationLoweredAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto f = MakeUnique<FunctionImplementationLoweredAst>(
        AstClone(TokL), AstCloneVec(Members), AstClone(TokR));
    f->SetScopePtr(_ScopePtr);
    return f;
}

auto spp::asts::FunctionImplementationLoweredAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    if (analyse::utils::builtins::BUILTIN_FUNCS.at(_ScopePtr).cmp_fn == nullptr) {
        return;
    }

    auto &lowered_cmp_code = *analyse::utils::builtins::BUILTIN_FUNCS.at(_ScopePtr).cmp_fn;
    auto extracted_args = Vec<Unique<ExpressionAst>>{};
    for (auto &&[_, arg] : std::move(meta->CmpArgs)) {
        extracted_args.push_back(std::move(arg));
    }
    meta->CmpResult = lowered_cmp_code.invoke(std::move(extracted_args));

    // analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidComptimeOperationError>()
    //     .with_args(*this)
    //     .raises_from(sm->CurrentScope);
}

auto spp::asts::FunctionImplementationLoweredAst::Stage11_CodeGen(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    // Todo: inject raw llvm somehow. Maybe read from annotation args.
    return nullptr;
}

auto spp::asts::FunctionImplementationLoweredAst::SetScopePtr(
    Str const &scope_str)
    -> void {
    // Set the scope string for this lowered function implementation.
    _ScopePtr = scope_str;
}

SPP_MOD_END
