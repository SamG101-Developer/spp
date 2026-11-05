module;
#include <spp/macros.hpp>

export module spp.asts.type_identifier_ast;
import spp.analyse.scopes.type_key;
import spp.asts.ast_kind;
import spp.asts.type_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(TypeIdentifierAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct ConventionAst);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts::meta, enum class CompilerStage : std::uint8_t);

/// A type expression represented by a single type name; the
/// type analogue of the expression IdentifierAst.
SPP_EXP_CLS struct spp::asts::TypeIdentifierAst final : TypeAst {
  SPP_GCC_VTABLE_FIX
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

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

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

  SPP_ATTR_NODISCARD auto WithoutGns() const -> Shared<TypeAst> override;

  /// Record the template (or alias) this name's head names wherever it is read; see "_StampedTemplateId".
  auto StampTemplateId(const analyse::scopes::TypeId id) const noexcept -> void { _StampedTemplateId = id; }

  SPP_ATTR_NODISCARD auto WithGns(Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const -> bool override;

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

  /// Mark this name as written where a type pack ("Ts") may be named on its own: a generic argument ("Tup[Ts]") or a
  /// variadic parameter's element type ("..xs: Ts"). Anywhere else a type pack is "not a type" (E124, D2).
  auto MarkPackPosition() -> void;

  /// Mark this name as the "!" type, which "TypeEq" treats as
  /// fitting any type. Set on the std "Never" class's own
  /// name, on "!" and on an alias of either, so every
  /// qualified reference built from those names carries it.
  auto MarkNeverType() -> void;

  /// Infer this type's arguments, at its next analysis, from what
  /// an object initializer gives each attribute (named first)
  /// against the attribute's declared type (last): "Box(val=1)"
  /// is a "Box[S32]". Handed to this name alone, so no other type
  /// analysed on the way infers from them.
  auto InferFromAttributes(
    Vec<Tup<Shared<IdentifierAst>, Shared<TypeAst>, Shared<TypeAst>>> &&equations)
    -> void;

  /// Run the visibility checks that names written in a signature
  /// or an attribute type deferred (see "_CheckVisibility"), now
  /// every class's visibility is on its symbol. Called once stage 5
  /// has finished for every module.
  static auto CheckPendingVisibility(ScopeManager *sm) -> void;

  /// Forget every deferred check, as a compile starts: one that
  /// stopped early would leave scopes behind that are gone.
  static auto ClearPendingVisibility() -> void;

private:
  /// A written name's visibility check, deferred: a copy of the
  /// name (for the error's position, the node read may be a
  /// temporary), what it named, and where it was named from.
  struct PendingVisibility {
    Shared<TypeIdentifierAst> Written;
    TypeSymbol const *Sym;
    Scope *From;
  };

  /// A class's visibility reaches its symbol only in stage 5 (from
  /// its annotation, read in stage 4), so a name read before then
  /// is checked once stage 5 is done for every module.
  inline static Vec<PendingVisibility> _PendingVisibility = {};

  /// Check this written name can be named where "sm" is: a type
  /// declared in a "sup" block by the type-level rule, anything
  /// else by the module-level one.
  auto _CheckVisibility(
    TypeSymbol const &type_sym, ScopeManager *sm, CompilerMetaData *meta) const
    -> void;

  /// What "InferFromAttributes" was given, consumed by the next analysis.
  Vec<Tup<Shared<IdentifierAst>, Shared<TypeAst>, Shared<TypeAst>>> _AttributeEquations;

  std::size_t _Pos;

  bool _IsNeverType;

  bool _IsSelfType;

  /// Whether analysis has resolved this node: a later analysis
  /// resolves nothing again, and only makes the checks its stage
  /// newly allows ("_RecheckArgs"), which is how a prototype
  /// enforces generic constraints early enough to keep error
  /// ordering sensible.
  bool _HasAnalysed;

  /// Whether this node's value has settled: its generic
  /// arguments named, variants collapsed. Set when the full
  /// analysis completes, unlike "_HasAnalysed", which the early
  /// returns ("Self", a stamped name) set too. Anything derived
  /// from the spelling (its cached string) keys off this.
  bool _Resolved;

  bool _IsSourceWritten;

  bool _IsPackPosition = false;

  /// The symbol the name's head resolved to (a template, alias or generic), kept for the checks a later analysis runs
  /// without resolving again ("_RecheckArgs"). Not cloned: a clone resolves afresh.
  TypeSymbol *_ResolvedSym = nullptr;

  /// Whether the arguments have been checked against their parameters: their constraints, from the pre-analysis stage
  /// unless a substituted type is read ("SkipSubstitutedConstraintChecks"), and comp values, once the sups are attached
  /// ("GenericSolver::Solve"). One resolved earlier is checked by a later analysis instead ("_RecheckArgs").
  bool _TypeArgsChecked = false;
  bool _CompArgsChecked = false;

  /// Whether the resolution was made before the sups are attached with an argument that needs them to be typed: a comp
  /// value that is no literal or name ("Wrap[Wrap[3_uz]::doubled]"), or a type argument resolved so itself. Its
  /// identity is then provisional, so the first analysis from the pre-analysis stage on resolves it again.
  bool _Provisional = false;

  /// "_Provisional", for this name's resolution as it was just made.
  SPP_ATTR_NODISCARD auto _ResolvedProvisionally(CompilerMetaData const *meta) const -> bool;

  /// The latest stage "_RecheckArgs" ran at: it runs once per stage, as that is when a check can newly become due.
  meta::CompilerStage _RecheckedAt = {};

  /// Analyse the (named) generic arguments: abstract types allowed, as naming one produces no value of it, and borrows
  /// too under a type that holds no value of its arguments ("names_only").
  auto _AnalyseArgs(bool names_only, ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Whether "type_sym" holds no value of the arguments written here, so a borrow among them is not stored ("Gen",
  /// "FunRef"). Only asked once the sups are attached, and only when a borrow may be written in them.
  auto _NamesOnly(TypeSymbol &type_sym, ScopeManager *sm, CompilerMetaData *meta) const -> bool;

  /// On an analysis after the one that resolved this name: the nested arguments' own checks, then the argument checks
  /// that were not yet due when it was resolved, by solving again over the arguments it named. Nothing is resolved again.
  auto _RecheckArgs(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// The unbound type pack this name, written as a type on its own, resolved to: E124 wherever it is analysed again
  /// ("Stage7_AnalyseSemantics"). Not cloned, as an instance binds the pack.
  TypeSymbol const *_TypePack = nullptr;

  /// The identity of the template (or alias) this name's head resolved to where it was analysed with arguments ("Alloc"
  /// of "Alloc[T]"). Kept through "Clone", "SubstituteSelf" and "WithGns", and handed to "WithoutGns", so the stripped
  /// name resolves to that declaration from anywhere ("Scope::FindBoundTypeSymbolById") rather than by its spelling.
  mutable analyse::scopes::TypeId _StampedTemplateId = nullptr;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeIdentifierAst)
