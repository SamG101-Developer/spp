module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);

/// A generic argument: a type, passed like "std::Vec[Str]", or
/// a compile time value, passed like "std::Arr[Str, 100_uz]".
/// Either is given by keyword ("T=Str") or by position. Exactly
/// one of "TypeVal" and "CompVal" is set.
SPP_EXP_CLS struct spp::asts::GenericArgumentAst final : Ast, mixins::OrderableAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericArgumentAst);

  /// The name of a keyword argument, used to refer to the
  /// argument in the generic call. Null for a positional one.
  Shared<TypeAst> Name;

  /// The "=" token separating a keyword argument's name from
  /// its value. Null for a positional one.
  Unique<TokenAst> TokAssign;

  /// The value of a type argument. Null for a comp argument.
  Shared<TypeAst> TypeVal;

  /// The value of a comp argument. Any type is allowed, as any
  /// type can be represented at compile time. Null for a type
  /// argument.
  Unique<ExpressionAst> CompVal;

  static auto NewType(decltype(Name) name, decltype(TypeVal) val) -> Unique<GenericArgumentAst>;

  static auto NewComp(decltype(Name) name, decltype(CompVal) &&val) -> Unique<GenericArgumentAst>;

  static auto FromSym(TypeSymbol const &sym) -> Unique<GenericArgumentAst>;

  static auto FromSym(VariableSymbol const &sym) -> Unique<GenericArgumentAst>;

  GenericArgumentAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(TypeVal) type_val,
    decltype(CompVal) &&comp_val);

  ~GenericArgumentAst() override;

  auto operator==(GenericArgumentAst const &other) const -> bool;

  /// The value, of whichever kind this argument is.
  SPP_ATTR_NODISCARD auto Value() const -> ExpressionAst*;

  /// The keyword name, or "" for a positional argument.
  SPP_ATTR_NODISCARD auto ViewName() const -> StrView;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

private:
  /// Analyse a type value and rewrite it as its qualified name,
  /// so it reads the same from a module that never imports it.
  /// A "Self" outside a function body is kept, for the caller
  /// to decide per use; a value naming a generic keeps its own
  /// node and stamp, and is resolved on read instead.
  auto AnalyseTypeVal(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Analyse a comp value: one that folds is checked against its
  /// type's bounds and not analysed further, anything else is
  /// analysed (an operator expression on a copy), and every comp
  /// generic it names is stamped with the parameter it means
  /// here.
  auto AnalyseCompVal(ScopeManager *sm, CompilerMetaData *meta) -> void;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentAst)
