module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterAst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericParameterTypeInlineConstraintsAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// A generic parameter on a class, function, superimposition
/// etc: a type ("T", "T: Copy", "T = Str", "..Ts") or a compile
/// time value ("cmp n: USize", "cmp n: USize = 1_uz", "cmp ..n:
/// USize"). A comp parameter is one with a "CompType"; a variadic
/// one has a "TokEllipsis", and an optional one a default of its
/// kind.
SPP_EXP_CLS struct spp::asts::GenericParameterAst final : Ast, mixins::OrderableAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericParameterAst);

  /// The "cmp" token of a comp parameter. Null for a type one.
  Unique<TokenAst> TokCmp;

  /// The ".." token of a variadic parameter, which can accept
  /// multiple values. Null otherwise.
  Unique<TokenAst> TokEllipsis;

  /// The name of the generic parameter, used to refer to it
  /// inside the generic type.
  Shared<TypeAst> Name;

  /// The inline constraints of a type parameter. In "fun
  /// func[T: Copy]()", "Copy" constrains "T". Null for a comp
  /// parameter.
  Unique<GenericParameterTypeInlineConstraintsAst> Constraints;

  /// The ":" token separating a comp parameter's name from its
  /// type. Null for a type parameter.
  Unique<TokenAst> TokColon;

  /// The type of a comp parameter, such as "I32" or "F64", as
  /// the type must be known at compile time. Null for a type
  /// parameter.
  Shared<TypeAst> CompType;

  /// The "=" token separating an optional parameter from its
  /// default. Null otherwise.
  Unique<TokenAst> TokAssign;

  /// The default of an optional type parameter, used if the
  /// argument is not provided.
  Shared<TypeAst> TypeDefault;

  /// The default of an optional comp parameter, used if the
  /// argument is not provided.
  Unique<ExpressionAst> CompDefault;

  /// Whether the parameter was copied onto a method from its enclosing
  /// "sup" block. The method binds it per instantiation like its own, but
  /// it names the block's symbol, so it declares no symbol of its own.
  bool IsInherited = false;

  GenericParameterAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Name) name,
    decltype(Constraints) &&constraints,
    decltype(TokColon) &&tok_colon,
    decltype(CompType) comp_type,
    decltype(TokAssign) &&tok_assign,
    decltype(TypeDefault) type_default,
    decltype(CompDefault) &&comp_default);

  ~GenericParameterAst() override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_ResolveDeclarations(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto GetDummyScopes() const -> std::span<Scope* const>;

  static auto ClearDummyScopes() -> void;

private:
  inline static Vec<Unique<Ast>> _DummyScopeAsts = {};

  Vec<Scope*> _DummyScopes;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericParameterAst)
