module;
#include <spp/analyse/macros.hpp>

module spp.asts.function_implementation_lowered_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.builtins;
import spp.asts.expression_ast;
import spp.asts.meta.compiler_meta_data;
import spp.utils.traits;
import genex;


spp::asts::FunctionImplementationLoweredAst::~FunctionImplementationLoweredAst() = default;


auto spp::asts::FunctionImplementationLoweredAst::new_empty()
    -> std::unique_ptr<FunctionImplementationLoweredAst> {
    return std::make_unique<FunctionImplementationLoweredAst>();
}


auto spp::asts::FunctionImplementationLoweredAst::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = FunctionImplementationAst::clone().release()->to<FunctionImplementationAst>();
    auto f = std::make_unique<FunctionImplementationLoweredAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
    f->set_scope_str(m_scope_str);
    return f;
}


auto spp::asts::FunctionImplementationLoweredAst::stage_9_comptime_resolution(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    if (analyse::utils::builtins::BUILTIN_FUNCS.at(m_scope_str).cmp_fn == nullptr) {
        return;
    }

    auto &lowered_cmp_code = *analyse::utils::builtins::BUILTIN_FUNCS.at(m_scope_str).cmp_fn;
    auto extracted_args = std::vector<std::unique_ptr<ExpressionAst>>{};
    for (auto &&[_, arg] : std::move(meta->cmp_args)) {
        extracted_args.push_back(std::move(arg));
    }
    meta->cmp_result = lowered_cmp_code.invoke(std::move(extracted_args));

    // analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidComptimeOperationError>()
    //     .with_args(*this)
    //     .raises_from(sm->current_scope);
}


auto spp::asts::FunctionImplementationLoweredAst::stage_11_code_gen_2(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    // Todo: inject raw llvm somehow.
    return nullptr;
}


auto spp::asts::FunctionImplementationLoweredAst::set_scope_str(
    std::string const &scope_str)
    -> void {
    // Set the scope string for this lowered function implementation.
    m_scope_str = scope_str;
}
