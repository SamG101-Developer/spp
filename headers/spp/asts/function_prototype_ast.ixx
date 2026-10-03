module;
#include <spp/macros.hpp>

export module spp.asts.function_prototype_ast;
import spp.analyse.scopes.instance_key;
import spp.analyse.utils.annotation_utils;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_fn;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionPrototypeAst);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct FunctionImplementationAst);
use(spp::asts, struct FunctionParameterGroupAst);
use(spp::asts, struct GenExpressionAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionOperatorFunctionCallAst);
use(spp::asts, struct SupPrototypeExtensionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, class Scope);

/// The prototype of a function: its name, parameters and
/// return type. The body is held in the implementation ast.
/// This is further inherited into the subroutine and coroutine
/// prototypes, which add additional analysis checks.
SPP_EXP_CLS struct spp::asts::FunctionPrototypeAst : Ast, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
  SPP_AST_KEY_FUNCTIONS(FunctionPrototypeAst);

  /// Optional "!abstract_method" annotation, marking the
  /// function as abstract, so it must be implemented in
  /// subclasses.
  AnnotationAst *AbstractAnnotation;

  /// Optional "!virtual_method" annotation, marking the
  /// function as virtual, so it can be overridden in
  /// subclasses.
  AnnotationAst *VirtualAnnotation;

  /// Optional "!hot" or "!cold" annotation, marking the
  /// function as called frequently or infrequently, to guide
  /// optimisation.
  AnnotationAst *TemperatureAnnotation;

  /// Optional "!ffi" annotation. The function is not
  /// implemented because it binds to an ffi symbol, so the
  /// usual type checking rules can be suspended.
  AnnotationAst *FfiAnnotation;

  /// Optional "!intrinsic" annotation. The function is
  /// specially implemented with llvm, so the usual type
  /// checking rules can be suspended.
  AnnotationAst *BuiltinAnnotation;

  /// Optional "!test" annotation, marking the function as a
  /// unit test. This forces extra analysis and makes it
  /// non-callable from s++ code.
  AnnotationAst *TestAnnotation;

  /// Optional "!always_inline", "!inline" or "!noinline"
  /// annotation, controlling whether the function is inlined.
  Pair<AnnotationAst*, Str> InlineAnnotation;

  /// All annotations applied to this prototype, including the
  /// access modifiers, "!virtual_method", "!abstract_method"
  /// and "!hot"/"!cold".
  Vec<Unique<AnnotationAst>> Annotations;

  /// The optional "cmp" token, marking this as a compile-time
  /// function that can be evaluated in compile-time contexts.
  /// Only supported with subroutines, not coroutines, at
  /// present.
  Unique<TokenAst> TokCmp;

  /// The "fun" or "cor" keyword that starts the prototype.
  Unique<TokenAst> TokFun;

  /// The name of the function. Uniqueness is not strictly
  /// required, as overloading is supported.
  Shared<IdentifierAst> Name;

  /// The optional generic parameter group, omitted if the
  /// function does not use generics.
  Unique<GenericParameterGroupAst> GnParamGroup;

  /// The parameter group, which is always present.
  Unique<FunctionParameterGroupAst> FnParamGroup;

  /// The "->" token separating the parameters from the return
  /// type.
  Unique<TokenAst> TokArrow;

  /// The return type. This is required, and is never inferred
  /// from the expressions inside the function.
  Shared<TypeAst> ReturnType;

  /// The body of the function, containing the code executed
  /// when it is called.
  Unique<FunctionImplementationAst> Impl;

  /// For an instantiation of a variadic function, the tuple
  /// its call collapsed the trailing arguments into. The
  /// variadic parameter still declares one element ("..b: T"),
  /// which is what analysis works with, but what is actually
  /// passed - and so what the llvm signature and the mangled
  /// name are built from - is this tuple. Null on a
  /// non-variadic function and on the uninstantiated template.
  Shared<TypeAst> VariadicPackType;

  struct {
    Unique<FunctionImplementationAst> OriginalImpl;
  } Source;

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

  auto Stage1_PreProcess(Ast *ctx) -> void override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_ResolveDeclarations(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// The linkage name an "!ffi" function is declared under:
  /// the "symbol" argument of its annotation, which is the
  /// name the shared library actually exports. An ffi function
  /// is never mangled the S++ way, because the definition it
  /// binds to was compiled by something else entirely
  /// (effectively: extern "C"). Empty if this is not an ffi
  /// function.
  SPP_ATTR_NODISCARD auto GetFfiSymbolName() const -> Str;

  SPP_ATTR_NODISCARD auto GetLlvmFn() const -> Shared<codegen::LlvmFnWrapper>;

  /// The llvm context of the module this prototype was written
  /// in, which is the one module its definition may be emitted
  /// into. Stamped on during Stage10, whose walk visits every
  /// module with that module's own context.
  ///
  /// An instantiation minted after that walk carries no stamp
  /// of its own, so the answer is taken from the template it
  /// substitutes (see "GetNonGnImpl") - the template is
  /// what a substitution is registered against, and its
  /// module owns the pair. Without this, an instantiation
  /// first needed halfway through some other module's code
  /// generation would have its "llvm::Function" created in
  /// that module, making whichever caller got there first the
  /// accidental owner of the definition. Null if Stage10 has
  /// not run yet.
  SPP_ATTR_NODISCARD auto OwnerCtx() const -> codegen::LlvmCtx*;

  /// The scope this prototype's signature is read in: a substitution's own ("GenericSubstitution::ProtoScope", where
  /// its bindings and "Self" are), else the prototype's. Where a declaration made outside the walk is positioned.
  SPP_ATTR_NODISCARD auto GetDeclarationScope() const -> Scope*;

  auto DetachLlvmFnSlot() -> void;

  SPP_ATTR_NODISCARD SPP_ATTR_COLD auto PrintSignature(Str const &owner) const -> Str;

  /// One generic instantiation of this prototype: the scope it lives in and the substituted prototype itself, found
  /// again by the identity of the arguments it was built from ("ArgsId") - without which every resolution of the same
  /// call would mint a fresh, indistinguishable substitution, and the declaration generated for one would not be
  /// findable from another.
  struct GenericSubstitution {
    Unique<Scope> OwnedScope;
    Unique<FunctionPrototypeAst> Proto;
    bool IsConcrete = false;
    bool IsBodyAnalysed = false;

    /// The identity of the arguments ("Scope::ArgsIdOf"),
    /// which is what finds this instantiation again: "f[T=T]" called
    /// from two generic contexts is two instantiations, one per "T".
    TypeId ArgsId;

    /// Whether a call has been resolved to it. A substitution is
    /// built to check a call against one overload before it is
    /// known which overload the call makes; one no call chose is
    /// kept (it is found again for the same arguments, and what
    /// its analysis registered may point into its scope) but is
    /// never analysed, declared or emitted.
    bool IsRequired = false;

    /// The scope to position a scope manager on before running
    /// any stage over "Proto", and the scope every symbol this
    /// instantiation bound was registered into.
    SPP_ATTR_NODISCARD auto WalkScope() const -> Scope*;

    /// The prototype's own scope, one below "WalkScope". The
    /// mock block Stage1 builds superimposes a single function
    /// over a mock type, so the block has exactly the one child
    /// scope and it is this.
    SPP_ATTR_NODISCARD auto ProtoScope() const -> Scope*;
  };

  /// File a built substitution against this prototype, not yet
  /// required ("GenericSubstitution::IsRequired").
  auto AddGnSubstitution(
    GenericSubstitution &&sub)
    -> GenericSubstitution&;

  /// Mark this prototype required when it is a substitution of a generic function ("GenericSubstitution::IsRequired"),
  /// and queue its template for the monomorphisation stage the first time. Called on whatever a call resolved to:
  /// nothing for a prototype that is no substitution.
  auto RequireGnSubstitution() const -> void;

  /// Find the substitution of this prototype whose arguments have
  /// this identity ("GenericSubstitution::ArgsId"), required
  /// or not, or nullptr if there is not one yet. Reusing the match
  /// keeps a call site and the Stage10 declaration walk pointing at
  /// a single prototype object for a given instantiation.
  SPP_ATTR_NODISCARD auto FindGnSubstitution(
    TypeId args_id)
    -> GenericSubstitution*;

  /// Analyse the bodies of every instantiation registered
  /// against this prototype that has not been analysed yet.
  /// The scope manager is used for its global scope only -
  /// each instantiation is analysed through a manager rooted
  /// at its own scope.
  auto AnalysePendingGnSubstitutions(ScopeManager *sm, CompilerMetaData *meta) -> void;

  static auto AnalysePendingDefaults(ScopeManager *sm) -> void;
  static auto ClearPendingDefaults() -> void;

  auto SetNonGnImpl(FunctionPrototypeAst *impl) -> void;

  SPP_ATTR_NODISCARD auto GetNonGnImpl() const -> FunctionPrototypeAst*;

  auto MarkAsAnnotation() -> void;

  SPP_ATTR_NODISCARD auto GetAnnotationInfo() const -> analyse::utils::annotation_utils::AnnotationInfo*;

  virtual auto GenerateLlvmDeclaration(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> Shared<codegen::LlvmFnWrapper>;

  virtual auto IsCoroutine() const -> bool = 0;

protected:
  /// What "Clone" copies besides the children it constructs the copy from: the analysed annotation state, the
  /// original body, the context, scope and lowered function, and the variadic pack type. Re-parents the copy's
  /// annotations onto it.
  auto _CloneStateInto(FunctionPrototypeAst &ast) const -> void;

  /// Using a list because there are times that the collection
  /// is iterated whilst being appended to.
  std::list<GenericSubstitution> _GnSubstitutions;

  FunctionPrototypeAst *_NonGnImpl;

  /// The LLVM generated function for this prototype. This is
  /// set during the first pass of code generation, and used
  /// for further codegen in the second pass (function calls
  /// etc). Double shared pointer for altering the value, and
  /// updating in cloned ASTs that share this target.
  Shared<Shared<codegen::LlvmFnWrapper>> _LlvmFn;

  /// The context of the module this prototype belongs to; read
  /// it through "OwnerCtx". Never set on a clone, because a
  /// clone is not written in any module - it is reached
  /// through the template it substitutes.
  codegen::LlvmCtx *_OwnerCtx;

  Unique<analyse::utils::annotation_utils::AnnotationInfo> _AnnotationInfo;

  inline static Vec<Pair<FunctionPrototypeAst*, bool>> _PendingDefaults = {};

  SPP_ATTR_NODISCARD auto _DeduceMockClsType() const -> Pair<Shared<TypeAst>, Str>;

  /// Swap an "!intrinsic" function's parsed body for the
  /// lowered one that dispatches into "kBuiltinFuncs". Done
  /// for the prototype itself in Stage6, and any generic
  /// substitutions of it. The scope manager must be positioned
  /// on this prototype's own scope.
  auto _InstallLoweredImpl(ScopeManager *sm) -> void;

  /// Mint the destructor an instantiation of a builtin will call,
  /// if "sub_proto" (the instantiation just analysed) is a builtin
  /// whose lowering destroys one of its generics (its table entry's
  /// "DropsGn"). Nothing else needs this: an ordinary body
  /// names what it calls, and analysing it instantiates those, but
  /// a builtin has no S++ body at all - see
  /// "drop_utils::EnsureDropInstantiated". "tm" must be positioned
  /// inside the instantiation's own scope, where its generics are
  /// bound.
  static auto _EnsureDropsForBuiltin(
    FunctionPrototypeAst const &sub_proto,
    ScopeManager &tm,
    CompilerMetaData *meta)
    -> void;

  SPP_ATTR_NODISCARD auto _IsPureGn(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx const *ctx) const
    -> Tup<bool, llvm::Type*, Vec<llvm::Type*>>;

  /// Emit the body of every instantiation registered against
  /// this prototype into "ctx", the module owning this
  /// prototype - the walk that reached it came from there.
  /// Both exits from "Stage11_CodeGen" run this, because a
  /// template emits nothing of its own but is exactly where
  /// the instantiations that do are registered.
  auto _CodeGenGnSubstitutions(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> void;
};
