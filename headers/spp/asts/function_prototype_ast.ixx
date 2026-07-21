module;
#include <spp/macros.hpp>

export module spp.asts.function_prototype_ast;
import spp.analyse.utils.annotation_utils;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_func;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct FunctionImplementationAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenExpressionAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct SupPrototypeExtensionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

/**
 * The @c FunctionPrototypeAst represents the prototype of a function. It defines the structure of a function, including
 * its name, parameters, and return type. The body of the function is defined in the FunctionImplementationAst.
 *
 * @n
 * This ASt is further inherited into the SubroutinePrototypeAst and CoroutinePrototypeAst, which add additional
 * analysis checks.
 */
SPP_EXP_CLS struct spp::asts::FunctionPrototypeAst : Ast, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
    SPP_GCC_VTABLE_FIX
    /**
     * Optional @c \@abstractmethod annotation. This is used to indicate that the function is abstract and must be
     * implemented in subclasses.
     */
    AnnotationAst *AbstractAnnotation;

    /**
     * Optional @c \@virtualmethod annotation. This is used to indicate that the function is virtual and can be
     * overridden in subclasses.
     */
    AnnotationAst *VirtualAnnotation;

    /**
     * Optional @c \@hot or @c \@cold annotation. This is used to indicate that the function is a hot/cold function,
     * which means it is called frequently/infrequently and should be optimized for performance.
     */
    AnnotationAst *TemperatureAnnotation;

    /**
     * Optional @c \@ffi annotation. This is used to indicate that the function is not implemented, because it is being
     * used for ffi, and therefore the usual type checking rules can be suspended.
     */
    AnnotationAst *FfiAnnotation;

    /**
     * Optional @c \@compiler_builtin annotation. This is used to indicate that the function is specially implemented
     * with llvm, and therefore the usual type checking rules can be suspended.
     */
    AnnotationAst *BuiltinAnnotation;

    /**
     * Optional @c \@always_inline, @c \@inline, or @c \@no_inline annotation. This is used to indicate that the
     * function should be inlined, or not inlined, or always inlined.
     */
    AnnotationAst *InlineAnnotation;

    /**
     * The list of annotations that are applied to this function prototype. There are quite a lot of annotations that
     * can be applied here, including the typical access modifiers, but also @c \@virtualmethod, @c \@abstractmethod,
     * and @c \@hot/\@cold.
     */
    Vec<Unique<AnnotationAst>> Annotations;

    /**
     * The optional @c cmp token indicates that this function prototype is a compile-time function. This means that the
     * function can be evaluated at compile time, and can be used in compile-time contexts.
     * @note only supported with subroutines, not coroutines, at present.
     */
    Unique<TokenAst> TokCmp;

    /**
     * The @c fun or @c cor keyword that represents the start of the function prototype. This is used to indicate that a
     * function is being defined.
     */
    Unique<TokenAst> TokFun;

    /**
     * The name of the function prototype. This is the identifier that is used to refer to the function. Uniqueness is
     * not strictly required, as overloading is supported.
     */
    Shared<IdentifierAst> Name;

    /**
     * The optional generic parameter group for the function prototype. This is used to define generic types that the
     * function can use. This is not required, and can be omitted if the function does not use generics.
     */
    Unique<GenericParameterGroupAst> GnParamGroup;

    /**
     * The parameter group for the function prototype. This is used to define the parameters that the function takes.
     * This is required, and must be present in the function prototype.
     */
    Unique<FunctionParameterGroupAst> FnParamGroup;

    /**
     * The token that represents the arrow @c -> in the function prototype. This separates the parameters from the
     * return type.
     */
    Unique<TokenAst> TokArrow;

    /**
     * The return type of the function prototype. This is the type that the function will return. This is required, and
     * is never "inferrable" from the expressions inside the function.
     */
    Shared<TypeAst> ReturnType;

    /**
     * The implementation of the function prototype. This is the body of the function, and contains the actual code that
     * will be executed when the function is called.
     */
    Unique<FunctionImplementationAst> Impl;

    struct {
        Shared<TypeAst> OriginalReturnType;
        Unique<FunctionImplementationAst> OriginalImpl;
    } Source;

    /**
     * Construct the FunctionPrototypeAst with the arguments matching the members.
     * @param annotations The list of annotations that are applied to this function prototype.
     * @param tok_cmp The optional @c cmp token indicating that this function prototype is a compile-time function.
     * @param tok_fun The @c fun or @c cor keyword that represents the start of the function prototype.
     * @param name The name of the function prototype.
     * @param generic_param_group An optional generic parameter group for the function prototype.
     * @param param_group The parameter group for the function prototype.
     * @param tok_arrow The token that represents the arrow @c -> in the function prototype.
     * @param return_type The return type of the function prototype.
     * @param impl The implementation of the function prototype.
     */
    FunctionPrototypeAst(
        decltype(Annotations) &&annotations,
        decltype(TokCmp) &&tok_cmp,
        decltype(TokFun) &&tok_fun,
        decltype(Name) &&name,
        decltype(GnParamGroup) &&generic_param_group,
        decltype(FnParamGroup) &&param_group,
        decltype(TokArrow) &&tok_arrow,
        decltype(ReturnType) &&return_type,
        decltype(Impl) &&impl);

    ~FunctionPrototypeAst() override;

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

    SPP_ATTR_NODISCARD auto GetLlvmFunc() const -> Shared<codegen::LlvmFuncWrapper>;

    SPP_ATTR_NODISCARD auto PrintSignature(Str const &owner) const -> Str;

    auto RegisterGenericSubstitution(Unique<analyse::scopes::Scope> &&scope, Unique<FunctionPrototypeAst> &&new_ast) -> void;

    SPP_ATTR_NODISCARD auto RegisteredGenericSubstitutions() const -> std::list<Pair<analyse::scopes::Scope*, FunctionPrototypeAst*>>;

    SPP_ATTR_NODISCARD auto RegisteredGenericSubstitutions() -> std::list<Pair<Unique<analyse::scopes::Scope>, Unique<FunctionPrototypeAst>>>&;

    auto SetNonGenericImpl(FunctionPrototypeAst *impl) -> void;

    SPP_ATTR_NODISCARD auto GetNonGenericImpl() const -> FunctionPrototypeAst*;

    auto MarkAsAnnotation() -> void;

    SPP_ATTR_NODISCARD auto GetAnnotationInfo() const -> analyse::utils::annotation_utils::AnnotationInfo*;

    virtual auto GenerateLlvmDeclaration(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> Shared<codegen::LlvmFuncWrapper>;

    virtual auto IsCoroutine() const -> bool = 0;

protected:
    /**
     * Using a list because there are times that the collection is iterated whilst being appended to.
     */
    std::list<Pair<Unique<analyse::scopes::Scope>, Unique<FunctionPrototypeAst>>> _GenericSubstitutions;

    FunctionPrototypeAst *_NonGenericImpl;

    /**
     * The LLVM generated function for this prototype. This is set during the first pass of code generation, and used
     * for further codegen in the second pass (for function calls, etc). Double shared pointer for altering the value,
     * and updating in cloned ASTs that share this target.
     */
    Shared<Shared<codegen::LlvmFuncWrapper>> _LlvmFunc;

    Unique<analyse::utils::annotation_utils::AnnotationInfo> _AnnotationInfo;

    SPP_ATTR_NODISCARD auto _DeduceMockClassType() const -> Pair<Shared<TypeAst>, Str>;

    SPP_ATTR_NODISCARD auto _IsPureGeneric(ScopeManager *sm, codegen::LLvmCtx *ctx) const -> std::tuple<bool, llvm::Type*, Vec<llvm::Type*>>;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::FunctionPrototypeAst)
