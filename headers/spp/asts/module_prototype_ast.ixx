module;
#include <spp/macros.hpp>

export module spp.asts.module_prototype_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::compiler {
    SPP_EXP_CLS struct CompilerBoot;
}

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct ModulePrototypeAst;
    SPP_EXP_CLS struct ModuleImplementationAst;
}


/**
 * The ModulePrototypeAst represents a prototype for a module in the SPP language. It contains a the implementation of
 * the module.
 */
SPP_EXP_CLS struct spp::asts::ModulePrototypeAst final : virtual Ast {
    /**
     * The file path of the module prototype. This is interacted with by the compiler to resolve module imports. Not got
     * from parsing children AST nodes.
     */
    std::filesystem::path file_path = "";

    /**
     * The module implementation AST that this prototype represents.
     */
    std::unique_ptr<ModuleImplementationAst> impl;

    /**
     * Construct the ModulePrototypeAst with the given implementation.
     * @param[in] impl The module implementation AST that this prototype represents.
     */
    explicit ModulePrototypeAst(
        decltype(impl) &&impl);

    ~ModulePrototypeAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto name() const -> std::unique_ptr<IdentifierAst>;

    auto file_name() const -> std::unique_ptr<IdentifierAst>;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_code_gen_1(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
