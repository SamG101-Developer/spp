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
use(spp::asts, struct IdentifierAst);
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

  /// The "=" token separating a keyword argument's name from
  /// its value. Null for a positional one.
  Unique<TokenAst> TokAssign;

  /// The value of a type argument. Null for a comp argument.
  Shared<TypeAst> TypeVal;

  /// The value of a comp argument. Any type is allowed, as any
  /// type can be represented at compile time. Null for a type
  /// argument.
  Shared<ExpressionAst> CompVal;

  static auto NewType(Shared<TypeAst> name, decltype(TypeVal) val) -> Unique<GenericArgumentAst>;

  static auto NewComp(Shared<TypeAst> name, decltype(CompVal) val) -> Unique<GenericArgumentAst>;

  static auto FromSymbol(TypeSymbol const &sym) -> Unique<GenericArgumentAst>;

  static auto FromSymbol(VariableSymbol const &sym) -> Unique<GenericArgumentAst>;

  GenericArgumentAst(
    Shared<TypeAst> name,
    decltype(TokAssign) &&tok_assign,
    decltype(TypeVal) type_val,
    decltype(CompVal) comp_val);

  ~GenericArgumentAst() override;

  auto operator==(GenericArgumentAst const &other) const -> bool;

  /// Whether this is a type argument ("Str") or a comp one ("1_uz"): exactly one holds.
  SPP_ATTR_NODISCARD auto IsTypeArg() const -> bool;
  SPP_ATTR_NODISCARD auto IsCompArg() const -> bool;

  /// The name of a keyword argument of either kind ("T=Str",
  /// "n=1_uz"), held as a type. Null for a positional one.
  SPP_ATTR_NODISCARD auto KeywordName() const -> Shared<TypeAst> const&;

  /// "KeywordName" as an identifier, which is what a comp argument's
  /// name ("n=1_uz") means. Built once, on first read.
  SPP_ATTR_NODISCARD auto CompNameAsId() const -> Shared<IdentifierAst> const&;

  /// The value, of whichever kind this argument is.
  SPP_ATTR_NODISCARD auto Value() const -> ExpressionAst*;

  /// The keyword name, or "" for a positional argument.
  SPP_ATTR_NODISCARD auto ViewName() const -> StrView;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

private:
  /// The keyword name ("KeywordName"), and the identifier it is read
  /// as for a comp argument ("CompNameAsId"), built on first read.
  Shared<TypeAst> _KeywordName;
  mutable Shared<IdentifierAst> _CompNameAsId;

  /// Analyse a type value and rewrite it as its qualified name,
  /// so it reads the same from a module that never imports it.
  /// A "Self" outside a function body is kept, for the caller
  /// to decide per use; a value naming a generic keeps its own
  /// node and written identity, and is resolved on read instead.
  auto AnalyseTypeVal(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Analyse a comp value: one that folds is checked against its
  /// type's bounds and not analysed further, anything else is
  /// analysed (an operator expression on a copy), and every comp
  /// generic it names records the parameter it means
  /// here.
  auto AnalyseCompVal(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Whether the comp value itself is what stage 7 analysed
  /// ("AnalyseCompVal"): not one that folds (never analysed), nor
  /// an operator expression (analysed on a copy, as analysing it
  /// desugars it). Only that is checked again after.
  SPP_ATTR_NODISCARD auto IsCompValAnalysedInPlace(Scope const &scope) const -> bool;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentAst)
