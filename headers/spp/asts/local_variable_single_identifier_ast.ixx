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
use(spp::asts, struct LocalVariableSingleIdentifierAliasAst);
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

private:
  /// The symbol this binding's Stage 7 created, and the one its
  /// name held in the same scope just before. A scope holds one
  /// symbol per name, so after Stage 7 the name belongs to
  /// whichever binding declared it last, and a later stage
  /// looking a same-scope shadower's predecessor up by name
  /// would find the shadower. Only a binding that shadows one
  /// in its own scope uses these; every other binding is found
  /// by its name, as before. Neither is carried by "Clone".
  Shared<VariableSymbol> _Sym;
  Shared<VariableSymbol> _PrevSym;

  /// Whether this binding is one of a same-scope shadowing
  /// pair, on either side: it shadows a binding in its own
  /// scope, or a later binding in its own scope has taken the
  /// name. Only then are the saved symbols used.
  SPP_ATTR_NODISCARD auto _UsesSavedSymbol(ScopeManager const *sm) const -> bool;

  SPP_ATTR_NODISCARD auto _OwnSymbol(ScopeManager const *sm) const -> VariableSymbol*;

  /// Put back what the name meant before this binding, for its
  /// value to be analysed, checked or generated against.
  /// Answers with what to re-add once the value is done: this
  /// binding's own symbol.
  auto _ExposePreviousSymbol(ScopeManager *sm) const -> Shared<VariableSymbol>;
};
