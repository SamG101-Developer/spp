module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_extension_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(SupPrototypeExtensionAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct SupImplementationAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// A superimposition of a type over a type, used to "inherit"
/// a type. For example, to extend the "A" type with "B":
///
///   sup A ext B {
///       # Override any abstract or virtual methods from B here.
///   }
SPP_EXP_CLS struct spp::asts::SupPrototypeExtensionAst final : Ast, ModuleMemberAst, SupMemberAst {
  SPP_AST_KEY_FUNCTIONS(SupPrototypeExtensionAst);

  /// The "sup" keyword that starts the superimposition.
  Unique<TokenAst> TokSup;

  /// The generics available for this superimposition, used to
  /// superimpose over generic types (all generics must be used
  /// by the type being extended).
  Unique<GenericParameterGroupAst> GnParamGroup;

  /// The name of the type being extended. This type gains the
  /// superclass defined by this superimposition.
  Shared<TypeAst> Name;

  /// The "ext" keyword, indicating that an extension and a
  /// method block are being defined.
  Unique<TokenAst> TokExt;

  /// The superclass this type is being extended from. Its
  /// attributes and methods become available on the
  /// superimposed type.
  Shared<TypeAst> SuperClass;

  /// The body of the superimposition: the methods (each a
  /// FunctionPrototypeAst) being added to the type.
  Unique<SupImplementationAst> Impl;

  SupPrototypeExtensionAst(
    decltype(TokSup) &&tok_sup,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(Name) name,
    decltype(TokExt) &&tok_ext,
    decltype(SuperClass) super_class,
    decltype(Impl) &&impl);

  ~SupPrototypeExtensionAst() override;

  auto Stage1_PreProcess(Ast *ctx) -> void override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

  auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_ResolveDeclarations(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto CheckCyclicExtension(TypeSymbol const &sup_sym, Scope &check_scope) const -> void;

  auto CheckDoubleExtension(TypeSymbol const &cls_sym, Scope &check_scope) const -> void;

  auto CheckSelfExtension(Scope &check_scope) const -> void;
};
