module;
#include <spp/macros.hpp>

export module spp.asts.class_prototype_ast;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct ClassImplementationAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeStatementAst;
}


/**
 * The ClassPrototypeAst represents the prototype of a class in the abstract syntax tree. It defines the structure of a
 * class, including its name and any generic parameters it may have. The attributes are defined in the implementation
 * ast for this class, allowing for scoping rules to be made easier.
 */
SPP_EXP_CLS struct spp::asts::ClassPrototypeAst final : Ast, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The list of annotations that are applied to this class prototype. Typically, access modifiers in this context.
     */
    Vec<Unique<AnnotationAst>> Annotations;

    /**
     * The @c cls keyword that represents the start of the class prototype. This is used to indicate that a class is
     * being defined.
     */
    Unique<TokenAst> TokCls;

    /**
     * The name of the class prototype. This is the identifier that is used to refer to the class, and must be unique
     * within the scope.
     */
    Shared<TypeAst> Name;

    /**
     * An optional generic parameter group for the class prototype. This is used to define generic types that the class
     * can use.
     */
    Shared<GenericParameterGroupAst> GnParamGroup;

    /**
     * The list of class attributes that are defined on the class prototype. These are the properties that the class
     * will have, and can be accessed through instances of the class.
     */
    Unique<ClassImplementationAst> Impl;

    /**
     * Construct the ClassPrototypeAst with the arguments matching the members.
     * @param[in] annotations The list of annotations that are applied to this class prototype.
     * @param[in] tok_cls The @c cls keyword that represents the start of the class prototype.
     * @param[in] name The name of the class prototype.
     * @param[in] generic_param_group An optional generic parameter group for the class prototype.
     * @param[in] impl The list of class attributes that are defined on the class prototype.
     */
    ClassPrototypeAst(
        decltype(Annotations) &&annotations,
        decltype(TokCls) &&tok_cls,
        decltype(Name) name,
        decltype(GnParamGroup) &&generic_param_group,
        decltype(Impl) &&impl);

    ~ClassPrototypeAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto RegisterGenericSubstitution(analyse::scopes::Scope *scope, Unique<ClassPrototypeAst> &&new_ast) -> void;

    SPP_ATTR_NODISCARD auto GetRegisteredGenericSubstitutions() const -> Vec<Pair<analyse::scopes::Scope*, ClassPrototypeAst*>>;

    SPP_ATTR_NODISCARD auto GetClsSym() const -> Shared<analyse::scopes::TypeSymbol>;

private:
    Vec<Pair<analyse::scopes::Scope*, Unique<ClassPrototypeAst>>> _GenericSubstitutions;

    Shared<analyse::scopes::TypeSymbol> _ClsSym;

    auto _GenerateSymbols(ScopeManager *sm) -> analyse::scopes::TypeSymbol*;

    auto _FillLlvmLayout(ScopeManager const *sm, analyse::scopes::TypeSymbol const *type_sym, codegen::LLvmCtx *ctx) const -> void;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ClassPrototypeAst)
