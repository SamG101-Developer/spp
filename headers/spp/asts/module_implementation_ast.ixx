module;
#include <spp/macros.hpp>

export module spp.asts.module_implementation_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ModuleImplementationAst;
    SPP_EXP_CLS struct ModuleMemberAst;
}


/**
 * The ModuleImplementationAst represents the implementation of a module in the SPP language. It contains a list of
 * module members that define the functionality and structure of the module.
 */
SPP_EXP_CLS struct spp::asts::ModuleImplementationAst final : virtual Ast {
    /**
     * The list of module members in the implementation. This can include function implementations, class implementations,
     * and other module-level constructs.
     */
    std::vector<std::unique_ptr<ModuleMemberAst>> members;

    /**
     * Construct the ModuleImplementationAst with the arguments matching the members.
     * @param[in] members The list of module members in the implementation.
     */
    explicit ModuleImplementationAst(
        decltype(members) &&members);

    ~ModuleImplementationAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_1(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto stage_11_code_gen_2(ScopeManager *, CompilerMetaData *, codegen::LLvmCtx *) -> llvm::Value* override;
};
