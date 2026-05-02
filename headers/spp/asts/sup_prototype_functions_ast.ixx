module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_functions_ast;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupPrototypeFunctionsAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The SupPrototypeFunctionsAst represents a superimposition of methods over a type. This is used to add behavior to a
 * type. For example, to extend the @c std::Str type with additional methods, the following code can be used:
 * @code
 * sup std::Str {
 *     fun to_upper() -> std::Str { ... }
 * }
 * @endcode
 */
SPP_EXP_CLS struct spp::asts::SupPrototypeFunctionsAst final : Ast, ModuleMemberAst {
    /**
     * The @c sup keyword that represents the start of the superimposition. This is used to indicate that a type is
     * being extended with additional methods.
     */
    Unique<TokenAst> TokSup;

    /**
     * The generics available for this superimposition. This is used to superimpose over generic types (all generics
     * must be used by the type being extended).
     */
    Unique<GenericParameterGroupAst> GnParamGroup;

    /**
     * The name of the type that is being extended. This is the type that will gain the additional methods defined in
     * the body of this superimposition.
     */
    Shared<TypeAst> Name;

    /**
     * The body of the superimposition. This is a list of methods that are being added to the type. Each method is
     * defined as a FunctionPrototypeAst, which includes the method's name, parameters, and return type.
     */
    Unique<SupImplementationAst> Impl;

    /**
     * Construct the SupPrototypeFunctionsAst with the arguments matching the members.
     * @param tok_sup The @c sup keyword that represents the start of the superimposition.
     * @param generic_param_group The generics available for this superimposition.
     * @param name The name of the type that is being extended.
     * @param impl The body of the superimposition.
     */
    SupPrototypeFunctionsAst(
        decltype(TokSup) &&tok_sup,
        decltype(GnParamGroup) &&generic_param_group,
        decltype(Name) name,
        decltype(Impl) &&impl);

    ~SupPrototypeFunctionsAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto Stage11_CodeGen(ScopeManager *sm , CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
