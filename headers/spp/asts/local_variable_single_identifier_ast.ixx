module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_single_identifier_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableSingleIdentifierAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct LocalVariableSingleIdentifierAliasAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TokenAst);
use(spp::analyse::scopes, struct VariableSymbol);

/// A local variable defined by a single identifier, such as
/// "mut x", which defines a mutable local variable "x". It can
/// be nested inside other local variable asts, such as a tuple
/// destructure: "(mut x, y) = (1, 2)". As this ast is either a
/// parameter or part of a "let" statement, it will introduce a
/// symbol into the local scope.
SPP_EXP_CLS struct spp::asts::LocalVariableSingleIdentifierAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableSingleIdentifierAst);

  /// A convention can ONLY be attached via the "case"
  /// expression pattern matching system. This allows borrows
  /// to be introduced into an inner scope (the case branch
  /// scope), and is needed here as well as in
  /// "CasePatternVariantSingleIdentifierAst", for mutability
  /// checking reasons.
  Unique<ConventionAst> Conv;

  /// The optional "mut" token, nullptr if not provided. Marks
  /// the variable as modifiable after its initial assignment.
  Unique<TokenAst> TokMut;

  /// The name used to refer to the variable in the local
  /// scope. It will be saved against the symbol in the current
  /// scope's symbol table.
  Shared<IdentifierAst> Name;

  /// The optional alias, to refer to the variable by a
  /// different name. This is useful in destructuring, to
  /// prevent conflicting variables when types have the same
  /// attribute names: "case my_value is Some(val as alias)".
  Unique<LocalVariableSingleIdentifierAliasAst> Alias;

  LocalVariableSingleIdentifierAst(
    decltype(TokMut) &&tok_mut,
    decltype(Name) name,
    decltype(Alias) &&alias);

  ~LocalVariableSingleIdentifierAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>> override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;

  /// From here on, this binding declares "flow", a flow-typed copy
  /// of its symbol (a destructure narrowing what it indexes): what
  /// a later walk finds under the name, where the "let" is written.
  auto Narrow(Shared<VariableSymbol> const &flow, Scope &scope) -> void;

private:
  /// The symbol this binding's Stage 7 created. This helps
  /// avoid issues with shadowing / redeclaring; the true
  /// symbol is stamped onto this AST.
  Shared<VariableSymbol> _Symbol;

  /// This binding's symbol, without declaring it. Either read
  /// from the stamp, otherwise pull from the symbol table.
  SPP_ATTR_NODISCARD auto _Own(
    ScopeManager const *sm) const -> VariableSymbol*;

  /// The symbol this binding declares, before it is declared: its
  /// own, or for a clone walked over its original's scope (which
  /// has none), the one its original recorded there.
  SPP_ATTR_NODISCARD auto _Binding(
    ScopeManager const *sm) const -> VariableSymbol*;

  /// Declare this binding in the current scope, (add the var
  /// symbol into the scope). Future asts will see this symbol.
  auto _Declare(
    ScopeManager *sm) const -> VariableSymbol*;
};
