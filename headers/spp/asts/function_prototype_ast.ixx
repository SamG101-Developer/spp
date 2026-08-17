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
  SPP_EXP_CLS struct GenericArgumentGroupAst;
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
  Pair<AnnotationAst*, Str> InlineAnnotation;

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
   * is never "inferable" from the expressions inside the function.
   */
  Shared<TypeAst> ReturnType;

  /**
   * The implementation of the function prototype. This is the body of the function, and contains the actual code that
   * will be executed when the function is called.
   */
  Unique<FunctionImplementationAst> Impl;

  /**
   * For an instantiation of a variadic function, the tuple its call collapsed the trailing arguments into. The
   * variadic parameter still declares one element ("..b: T"), which is what analysis works with, but what is
   * actually passed - and so what the llvm signature and the mangled name have to be built from - is this tuple.
   * Null on a non-variadic function and on the uninstantiated template.
   */
  Shared<TypeAst> VariadicPackType;

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
    decltype(Name) name,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(FnParamGroup) &&param_group,
    decltype(TokArrow) &&tok_arrow,
    decltype(ReturnType) return_type,
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

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /**
   * The linkage name an @c \@ffi function is declared under: the @c symbol argument of its annotation, which is the
   * name the shared library actually exports. An ffi function is never mangled the S++ way, because the definition it
   * binds to was compiled by something else entirely (effectively: extern "C").
   * @return The symbol name, or an empty string if this is not an ffi function.
   */
  SPP_ATTR_NODISCARD auto GetFfiSymbolName() const
    -> Str;

  SPP_ATTR_NODISCARD auto GetLlvmFunc() const
    -> Shared<codegen::LlvmFuncWrapper>;

  /**
   * The llvm context of the module this prototype was written in, which is the one module its definition may be
   * emitted into. Stamped on during Stage10, whose walk visits every module with that module's own context.
   *
   * @n
   * An instantiation carries no stamp of its own if it was minted after that walk, so the answer is taken from the
   * template it substitutes (see @c GetNonGenericImpl ) - the template is what a substitution is registered against,
   * and it is the template's module that owns the pair. Without this, an instantiation first needed halfway through
   * some other module's code generation would have its @c llvm::Function created in that module, making whichever
   * caller got there first the accidental owner of the definition.
   *
   * @return The owning context, or @c nullptr if Stage10 has not run yet.
   */
  SPP_ATTR_NODISCARD auto OwnerCtx() const
    -> codegen::LlvmCtx*;

  auto DetachLlvmFuncSlot()
    -> void;

  SPP_ATTR_NODISCARD auto PrintSignature(
    Str const &owner) const
    -> Str;

  /**
   * One generic instantiation of this prototype: the scope it lives in, the substituted prototype itself, and the
   * generic arguments it was built from. The arguments are what give the instantiation a stable identity - without
   * them every resolution of the same call would mint a fresh, indistinguishable substitution, and the declaration
   * generated for one would not be findable from another.
   */
  struct GenericSubstitution {
    Unique<analyse::scopes::Scope> OwnedScope;
    Unique<FunctionPrototypeAst> Proto;
    Unique<GenericArgumentGroupAst> GnArgs;

    /**
     * Whether this instantiation names real types the whole way down - both the arguments it was built from and the
     * signature it ended up with. Decided once, where it is built (see
     * @c PotentiallyGenerateGenericSubstitutedPrototype ), because every later reader must reach the same answer:
     * declaration and emission disagreeing leaves a call with no target, and a body built against a type that has no
     * size produces ir that does not verify.
     *
     * @n
     * A false one is still built and still analysed - the call it came from is type checked against its signature -
     * it is simply never given an @c llvm::Function nor a body.
     */
    bool IsConcrete = false;

    /**
     * Whether the monomorphisation stage has already considered this instantiation. Set whether or not a body was
     * actually analysed, because an instantiation declined once (still generic, or never filled in) is declined for
     * good - the drain re-reads the whole list every time its template comes up, and this is what keeps that from
     * being quadratic and from re-analysing a body that is already analysed.
     */
    bool BodyAnalysed = false;

    /**
     * The scope to position a scope manager on before running any stage over @c Proto , and the scope every symbol
     * this instantiation bound was registered into.
     */
    SPP_ATTR_NODISCARD auto WalkScope() const
      -> analyse::scopes::Scope*;

    /**
     * The prototype's own scope, one below @c WalkScope . The mock block Stage1 builds superimposes a single function
     * over a mock type, so the block has exactly the one child scope and it is this.
     */
    SPP_ATTR_NODISCARD auto ProtoScope() const
      -> analyse::scopes::Scope*;
  };

  auto RegisterGenericSubstitution(
    Unique<analyse::scopes::Scope> &&scope,
    Unique<FunctionPrototypeAst> &&new_ast,
    Unique<GenericArgumentGroupAst> &&gn_args) -> void;

  /**
   * Find the instantiation of this prototype built from exactly these generic arguments, or @c {nullptr, nullptr} if
   * there is not one yet. Reusing the match is what keeps a call site and the Stage10 declaration walk pointing at a
   * single prototype object for a given instantiation.
   */
  SPP_ATTR_NODISCARD auto FindGenericSubstitution(
    GenericArgumentGroupAst const &gn_args) const
    -> Pair<analyse::scopes::Scope*, FunctionPrototypeAst*>;

  SPP_ATTR_NODISCARD auto RegisteredGenericSubstitutions() const
    -> std::list<Pair<analyse::scopes::Scope*, FunctionPrototypeAst*>>;

  SPP_ATTR_NODISCARD auto RegisteredGenericSubstitutions()
    -> std::list<GenericSubstitution>&;

  /**
   * Analyse the bodies of every instantiation registered against this prototype that has not been analysed yet.
   *
   * @n
   * An instantiation is built from the signature alone - @c PotentiallyGenerateGenericSubstitutedPrototype substitutes
   * the parameters and return type, because that is all overload resolution needs - so its body arrives here still
   * being the template's, written in terms of parameters this instantiation has since bound. Analysing it is what
   * turns it into this instantiation's body, and is also the only thing that discovers what *it* calls: every
   * instantiation reached only from inside another generic body exists because of this walk.
   *
   * @param[in] sm The scope manager, used for its global scope only - each instantiation is analysed through a manager
   * rooted at its own scope.
   * @param[in] meta The compiler meta data.
   */
  auto AnalysePendingGenericSubstitutions(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void;

  auto SetNonGenericImpl(
    FunctionPrototypeAst *impl)
    -> void;

  SPP_ATTR_NODISCARD auto GetNonGenericImpl() const
    -> FunctionPrototypeAst*;

  auto MarkAsAnnotation()
    -> void;

  SPP_ATTR_NODISCARD auto GetAnnotationInfo() const
    -> analyse::utils::annotation_utils::AnnotationInfo*;

  virtual auto GenerateLlvmDeclaration(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> Shared<codegen::LlvmFuncWrapper>;

  virtual auto IsCoroutine() const
    -> bool = 0;

protected:
  /**
   * Using a list because there are times that the collection is iterated whilst being appended to.
   */
  std::list<GenericSubstitution> _GenericSubstitutions;

  FunctionPrototypeAst *_NonGenericImpl;

  /**
   * The LLVM generated function for this prototype. This is set during the first pass of code generation, and used
   * for further codegen in the second pass (for function calls, etc). Double shared pointer for altering the value,
   * and updating in cloned ASTs that share this target.
   */
  Shared<Shared<codegen::LlvmFuncWrapper>> _LlvmFunc;

  /**
   * The context of the module this prototype belongs to; see @c OwnerCtx , which is how it should be read. Never set
   * on a clone, because a clone is not written in any module - it is reached through the template it substitutes.
   */
  codegen::LlvmCtx *_OwnerCtx;

  Unique<analyse::utils::annotation_utils::AnnotationInfo> _AnnotationInfo;

  SPP_ATTR_NODISCARD auto _DeduceMockClassType() const
    -> Pair<Shared<TypeAst>, Str>;

  /**
   * Swap a @c \@compiler_builtin function's parsed body for the lowered one that dispatches into @c kBuiltinFuncs .
   * Done for the prototype itself in Stage6, and any generic substitutions of it.
   * @param[in] sm The scope manager, positioned on this prototype's own scope.
   */
  auto _InstallLoweredImpl(
    ScopeManager *sm)
    -> void;

  SPP_ATTR_NODISCARD auto _IsPureGeneric(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx const *ctx) const
    -> Tup<bool, llvm::Type*, Vec<llvm::Type*>>;

  /**
   * Emit the body of every instantiation registered against this prototype into @p ctx , which is the module owning
   * this prototype - the walk that reached it came from there. Both exits from @c Stage11_CodeGen run this, because a
   * template emits nothing of its own but is exactly where the instantiations that do are registered.
   */
  auto _CodeGenGenericSubstitutions(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> void;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::FunctionPrototypeAst)
