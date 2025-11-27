module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_ast;
import spp.asts.primary_expression_ast;

import llvm;
import std;


SPP_EXP_CLS struct spp::asts::ObjectInitializerAst final : PrimaryExpressionAst {
    /**
     * The type being initialized by the object initializer. This is the type of the object being created.
     */
    std::shared_ptr<TypeAst> type;

    /**
     * The object initializer argument group that contains the arguments for the object initializer. These arguments
     * will be passed into the attributes of the object being created.
     */
    std::unique_ptr<ObjectInitializerArgumentGroupAst> arg_group;

    /**
     * Construct the ObjectInitializerAst with the arguments matching the members.
     * @param type The type being initialized by the object initializer.
     * @param arg_group The object initializer argument group that contains the arguments for the object initializer.
     */
    ObjectInitializerAst(
        decltype(type) type,
        decltype(arg_group) &&arg_group);

    ~ObjectInitializerAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
