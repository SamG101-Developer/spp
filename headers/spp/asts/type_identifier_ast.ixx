module;
#include <spp/macros.hpp>

export module spp.asts.type_identifier_ast;
import spp.asts.ast_kind;
import spp.asts.type_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(TypeIdentifierAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct IdentifierAst);

/// A type expression represented by a single type name; the
/// type analogue of the expression IdentifierAst.
SPP_EXP_CLS struct spp::asts::TypeIdentifierAst final : TypeAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypeIdentifierAst);

  /// The name of the type, such as "Str" or "Vec[BigInt]".
  Str Name;

  /// The generic arguments used to instantiate the type.
  Unique<GenericArgumentGroupAst> GnArgGroup;

  /// Create a TypeIdentifierAst from an IdentifierAst, using
  /// the identifier's internal string.
  static auto FromIdentifier(IdentifierAst const &identifier) -> Shared<TypeIdentifierAst>;

  /// Create a TypeIdentifierAst from a raw string. No parsing
  /// is done, so generics cannot be included in the string;
  /// use "ParserSpp" with the "INJECT_CODE" macro for that.
  static auto FromString(Str const &identifier) -> Shared<TypeIdentifierAst>;

  explicit TypeIdentifierAst(
    std::size_t pos,
    decltype(Name) &&name,
    decltype(GnArgGroup) generic_arg_group);

  ~TypeIdentifierAst() override;

  auto operator<=>(const TypeIdentifierAst &that) const -> Ordering;
  auto operator==(const TypeIdentifierAst &that) const -> bool;

  SPP_ATTR_NODISCARD auto EqualsTypeIdentifier(TypeIdentifierAst const &other) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto AnyPart(std::function<bool(TypeIdentifierAst const &)> const &pred) const -> bool override;

  SPP_ATTR_NODISCARD auto IsNeverType() const noexcept -> bool override;

  SPP_ATTR_NODISCARD auto IsSelfType() const noexcept -> bool override;

  auto NsPartsInto(Vec<IdentifierAst const*> &out) const -> void override;

  auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const -> void override;

  SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto NsParts() -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto TypeParts() -> Vec<TypeIdentifierAst*> override;

  SPP_ATTR_NODISCARD auto LastTypePart() const -> TypeIdentifierAst const* override;

  SPP_ATTR_NODISCARD auto LastTypePart() -> TypeIdentifierAst* override;

  SPP_ATTR_NODISCARD auto WithoutConvention() const -> Shared<const TypeAst> override;

  SPP_ATTR_NODISCARD auto GetConvention() const -> ConventionAst* override;

  SPP_ATTR_NODISCARD auto WithConvention(Unique<ConventionAst> &&conv) const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto WithoutGenerics() const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenerics(Vec<GenericArgumentAst*> const &args) const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ContainsGenerics(GenericParameterAst const &generic) const -> bool override;

  SPP_ATTR_NODISCARD auto WithGenerics(Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const -> bool override;

  auto ResetCache() -> void override;

  SPP_ATTR_NODISCARD auto IsTypeIdentifier() const noexcept -> bool override;

  auto AnkerlHash() const -> std::size_t override;

  SPP_ATTR_NODISCARD auto ToView() const -> StrView;

  /// Forget that this type was written in source. A type made
  /// by substituting a generic argument is a clone of whatever
  /// the caller named, so it carries the caller's flag;
  /// analysed inside the template's module it then reads as an
  /// access written there, and a caller's own private type is
  /// reported illegal from inside std. Only what someone
  /// actually typed at a site is an access by that site.
  auto ClearSourceWritten() -> void;

  auto MarkSourceWritten() -> void;

  /// Mark this name as the "!" type, which "TypeEq" treats as
  /// fitting any type. Set on the std "Never" class's own
  /// name, on "!" and on an alias of either, so every
  /// qualified reference built from those names carries it.
  auto MarkNeverType() -> void;

private:
  std::size_t _Pos;

  bool _IsNeverType;

  bool _IsSelfType;

  /// Whether analysis has run over this node, used to skip a
  /// second run. Cleared by "ResetCache" so a type can be
  /// analysed again at a different stage, which several
  /// prototypes do to enforce generic constraints early enough
  /// to keep error ordering sensible.
  bool _HasAnalysed;

  /// Whether this node's value has settled: its generic
  /// arguments named, "Self" resolved, variants collapsed. Set
  /// when analysis completes and, unlike "_HasAnalysed", never
  /// cleared afterwards: re-analysis re-runs the stage checks,
  /// it does not un-settle what the type is. Anything derived
  /// from the type keys off this, so a forced re-analysis does
  /// not retire work that is still correct.
  bool _Resolved;

  bool _IsSourceWritten;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeIdentifierAst)
