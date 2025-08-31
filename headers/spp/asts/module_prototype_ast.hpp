#pragma once

#include <filesystem>
#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ModulePrototypeAst represents a prototype for a module in the SPP language. It contains a the implementation of
 * the module.
 */
struct spp::asts::ModulePrototypeAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

private:
    std::filesystem::path m_file_path;

public:
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

    auto name() const -> std::unique_ptr<IdentifierAst>;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
