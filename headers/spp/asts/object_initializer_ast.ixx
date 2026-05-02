module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentGroupAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::ObjectInitializerAst final : PrimaryExpressionAst {
    /**
     * The type being initialized by the object initializer. This is the type of the object being created.
     */
    Shared<TypeAst> Type;

    /**
     * The object initializer argument group that contains the arguments for the object initializer. These arguments
     * will be passed into the attributes of the object being created.
     */
    Unique<ObjectInitializerArgumentGroupAst> ArgGroup;

    /**
     * Construct the ObjectInitializerAst with the arguments matching the members.
     * @param type The type being initialized by the object initializer.
     * @param arg_group The object initializer argument group that contains the arguments for the object initializer.
     */
    ObjectInitializerAst(
        decltype(Type) type,
        decltype(ArgGroup) &&arg_group);

    ~ObjectInitializerAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
