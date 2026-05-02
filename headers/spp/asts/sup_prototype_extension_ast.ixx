module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_extension_ast;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupPrototypeExtensionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The SupPrototypeExtensionAst represents a superimposition of a type over a type. This is used to "inherit" a type.
 * For example, to extend the @c A type with @c B, the following code can be used:
 * @code
 * sup A ext B {
 *     # Override any abstract or virtual methods from B here.
 * }
 * @endcode
 */
SPP_EXP_CLS struct spp::asts::SupPrototypeExtensionAst final : Ast, ModuleMemberAst, SupMemberAst {
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
     * The @c ext keyword that represents the type that is being extended. This is used to indicate that an extension,
     * and a method block, is being defined.
     */
    Unique<TokenAst> TokExt;

    /**
     * The name of the super class that is this type is being extended from. The attributes and methods of this type
     * will now be available on the superimposed type.
     */
    Shared<TypeAst> SuperClass;

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
     * @param tok_ext The @c ext keyword that represents the type that is being extended.
     * @param super_class The name of the super class that is this type is being extended from.
     * @param impl The body of the superimposition.
     */
    SupPrototypeExtensionAst(
        decltype(TokSup) &&tok_sup,
        decltype(GnParamGroup) &&generic_param_group,
        decltype(Name) name,
        decltype(TokExt) &&tok_ext,
        decltype(SuperClass) super_class,
        decltype(Impl) &&impl);

    ~SupPrototypeExtensionAst() override;

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

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto CheckCyclicExtension(
        analyse::scopes::TypeSymbol const &sup_sym,
        analyse::scopes::Scope &check_scope) const
        -> void;

    auto CheckDoubleExtension(
        analyse::scopes::TypeSymbol const &cls_sym,
        analyse::scopes::Scope &check_scope) const
        -> void;

    auto CheckSelfExtension(
        analyse::scopes::Scope &check_scope) const
        -> void;
};
