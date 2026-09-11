module;
#include <spp/macros.hpp>

export module spp.asts.type_identifier_ast;
import spp.asts.ast_kind;
import spp.asts.type_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(TypeIdentifierAst) {
  SPP_EXP_CLS struct ConventionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct GenericParameterAst;
  SPP_EXP_CLS struct IdentifierAst;
}

/**
 * The TypeIdentifierAst is a type expression that is represented by a single type name, and is analogous to the
 * IdentifierAst of the ExpressionAst.
 */
SPP_EXP_CLS struct spp::asts::TypeIdentifierAst final : TypeAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypeIdentifierAst);

  /**
   * The name for the type. This is the name of the type, such as @c Str or @code Vec[BigInt]@endcode.
   */
  Str Name;

  /**
   * The generic arguments for the type. This is a list of generic arguments that are used to instantiate the type.
   */
  Unique<GenericArgumentGroupAst> GnArgGroup;

  /**
   * Factory function to create a @c TypeIdentifierAst from an @c IdentifierAst node. This uses the internal string
   * inside of the @c IdentifierAst to construct the @C TypeIdentifierAst.
   * @param identifier The @c IdentifierAst node being transitioned.
   * @return The new @c TypeIdentifierAst.
   */
  static auto FromIdentifier(IdentifierAst const &identifier) -> Shared<TypeIdentifierAst>;

  /**
   * Factory function to create a @c TypeIdentifierAst from a raw @c Str node. Generics cannot be included in
   * the string as no parsing is done, just wrapping into the @C TypeIdentifierNode. Use the @c ParserSpp class
   * otherwise, with the @c INJECT_CODE macro, to parse generics.
   * @param identifier The raw string being transitioned.
   * @return The new @c TypeIdentifierAst.
   */
  static auto FromString(Str const &identifier) -> Shared<TypeIdentifierAst>;

  /**
   * Construct the TypeIdentifier with the arguments matching the members.
   * @param[in] pos The position of the type in the source code.
   * @param[in] name The name for the type.
   * @param[in] generic_arg_group The generic arguments for the type.
   */
  explicit TypeIdentifierAst(
    std::size_t pos,
    decltype(Name) &&name,
    decltype(GnArgGroup) generic_arg_group);

  ~TypeIdentifierAst() override;

  auto operator<=>(const TypeIdentifierAst &that) const -> Ordering;
  auto operator==(const TypeIdentifierAst &that) const -> bool;

  SPP_ATTR_NODISCARD auto EqualsTypeIdentifier(TypeIdentifierAst const &other) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto AnyPart(
    std::function<bool(TypeIdentifierAst const &)> const &pred) const -> bool override;

  SPP_ATTR_NODISCARD auto IsNeverType() const noexcept
    -> bool override;

  SPP_ATTR_NODISCARD auto IsSelfType() const noexcept
    -> bool override;

  auto NsPartsInto(Vec<IdentifierAst const*> &out) const
    -> void override;

  auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const
    -> void override;

  SPP_ATTR_NODISCARD auto NsParts() const
    -> Vec<IdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto NsParts()
    -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto TypeParts() const
    -> Vec<TypeIdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto TypeParts()
    -> Vec<TypeIdentifierAst*> override;

  SPP_ATTR_NODISCARD auto LastTypePart() const
    -> TypeIdentifierAst const* override;

  SPP_ATTR_NODISCARD auto LastTypePart()
    -> TypeIdentifierAst* override;

  SPP_ATTR_NODISCARD auto WithoutConvention() const
    -> Shared<const TypeAst> override;

  SPP_ATTR_NODISCARD auto GetConvention() const
    -> ConventionAst* override;

  SPP_ATTR_NODISCARD auto WithConvention(
    Unique<ConventionAst> &&conv) const
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto WithoutGenerics() const
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenerics(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ContainsGenerics(
    GenericParameterAst const &generic) const
    -> bool override;

  SPP_ATTR_NODISCARD auto WithGenerics(
    Unique<GenericArgumentGroupAst> &&arg_group) const
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const
    -> bool override;

  auto ResetCache()
    -> void override;

  SPP_ATTR_NODISCARD auto IsTypeIdentifier() const noexcept
    -> bool override;

  auto AnkerlHash() const
    -> std::size_t override;

  SPP_ATTR_NODISCARD auto ToView() const
    -> StrView;

  /**
   * Forget that this type was written in source. A type produced by substituting a generic argument is a clone of
   * whatever the caller named, so it arrives carrying the caller's flag; analysed inside the template's module it
   * then reads as an access written there, and a caller's own private type is reported illegal from inside std.
   * Only what someone actually typed at a site is an access by that site.
   */
  auto ClearSourceWritten() -> void;

  auto MarkSourceWritten()
    -> void;

private:
  std::size_t _Pos;

  bool _IsNeverType;

  bool _IsSelfType;

  /**
   * Whether analysis has run over this node, used to skip a second run. Cleared by @c ResetCache so that a type can be
   * analysed again at a different stage, which several prototypes do to enforce generic constraints early enough to
   * keep error ordering sensible.
   */
  bool _HasAnalysed;

  /**
   * Whether this node's @e value has settled - its generic arguments named, @c Self resolved, variants collapsed. Set
   * when analysis completes and, unlike @c _HasAnalysed , never cleared afterwards: re-analysis re-runs the stage
   * checks, it does not un-settle what the type is. Anything derived from the type keys off this, so that a forced
   * re-analysis does not retire work that is still correct.
   */
  bool _Resolved;

  bool _IsSourceWritten;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeIdentifierAst)
