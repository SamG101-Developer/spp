module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.instance_key;
import spp.utils.interner;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct CompNode);
use(spp::analyse::scopes, struct TypeSymbol);

namespace spp::analyse::scopes {
  SPP_EXP_CLS struct TypeIdHead;
  SPP_EXP_CLS struct TypeIdParams;
  using Word = std::uint64_t;

  /// Intern a string (the "text") by using the internal interner
  /// for all strings, and casting the result to an integer. The
  /// forward "text->id" conversion entrance api.
  SPP_EXP_FUN inline auto InternWord(const StrView text) -> Word {
    return static_cast<Word>(utils::Intern(text));
  }

  /// De-intern a number (the "word") by using the internal
  /// interner for all ids, after casting the id to an intern
  /// identifier. The reverse "id->text" conversion entrance api.
  SPP_EXP_FUN inline auto WordText(const Word word) -> StrView {
    return utils::InternedText(static_cast<utils::InternedId>(word));
  }

  /// The "instance key" is an identity that an instantiation is
  /// tagged as. It holds what each argument resolved to (as ids
  /// themselves) and their shapes. Used for generics everywhere.
  SPP_EXP_CLS struct InstanceKey {
    enum class Tag : std::uint8_t {
      Name, Pos, Conv, Unresolved, TypeBound, TypeParam, Self, Symbol, CompId, Inst, Variant, Len, TypeId,
      CompParam, TypePack, CompPack
    };

    /// The interned parts of the instance information. This might
    /// include things like generic argument interned information.
    std::vector<Word> Words;

    /// The "intern" of this instantiation. This is the master value
    /// used for things like comparisons.
    std::uint64_t Hash = 0;

    /// Whether any part of the key names something that did not
    /// resolve, which is no type at all: two such keys spelled alike
    /// are equal as keys, never as types.
    bool HasUnresolved = false;

    /// Whether any part of the key is "Self", which is keyed by its
    /// spelling: what it means depends on the scope reading it.
    bool HasSelf = false;

    /// What "HeadOf", "ParamsOf" and "BareTypeId" read off an
    /// interned key, kept on it: an interned key never changes, so
    /// each is worked out once. Not part of the key.
    mutable TypeIdHead const *CachedHead = nullptr;
    mutable TypeIdParams const *CachedParams = nullptr;
    mutable InstanceKey const *CachedBare = nullptr;

    /// Append a tag with a small payload (an id, a position, a
    /// convention). This is the root payload push algorithm that
    /// should be wrapped. It itself uses "Append" (raw).
    auto Push(const Tag tag, const std::uint64_t payload = 0) -> void {
      HasUnresolved = HasUnresolved or tag == Tag::Unresolved;
      HasSelf = HasSelf or tag == Tag::Self;
      Append(static_cast<std::uint64_t>(tag) << 56 | payload);
    }

    /// Append a tag with text that is keyed by its spelling, by
    /// first interning the text and then pushing the id.
    auto PushText(const Tag tag, const StrView text) -> void {
      Push(tag, InternWord(text));
    }

    /// Append a symbol's identity: its address. Reinterpret the
    /// address as an id and push that as a number.
    auto PushPtr(void const *const ptr) -> void {
      Push(Tag::Symbol);
      Append(reinterpret_cast<std::uintptr_t>(ptr));
    }

    /// Append an interned type ("TypeId") as one part of this key:
    /// the one word stands for its whole key, as interning makes
    /// equal keys one address.
    auto PushId(InstanceKey const *const id) -> void {
      HasSelf = HasSelf or id->HasSelf;
      Push(Tag::TypeId);
      Append(reinterpret_cast<std::uintptr_t>(id));
    }

    /// Append an interned comp value ("CompId") as one part of
    /// this key: its tag, then its address, as "PushId" does for
    /// a type; interning makes equal values one address.
    auto PushCompId(CompNode const *const id) -> void {
      Push(Tag::CompId);
      Append(reinterpret_cast<std::uintptr_t>(id));
    }

    /// Append a whole key, as one part of this one: its length
    /// first, so where it ends is part of the key.
    auto PushKey(InstanceKey const &that) -> void {
      Push(Tag::Len, that.Words.size());
      AppendKey(that);
    }

    /// Append words already laid out as a key's, one part of
    /// another key copied as it stands: "unresolved" says whether
    /// they namesomething that did not resolve. Loop and append.
    auto AppendWords(const std::span<const Word> words, const bool unresolved,
      const bool self = false) -> void {
      HasUnresolved = HasUnresolved or unresolved;
      HasSelf = HasSelf or self;
      for (const auto word : words) { Append(word); }
    }

    /// Continue this key with another's words, as though they
    /// had been pushed here: the key of what a type stands for.
    auto AppendKey(InstanceKey const &that) -> void {
      HasUnresolved = HasUnresolved or that.HasUnresolved;
      HasSelf = HasSelf or that.HasSelf;
      for (const auto word : that.Words) { Append(word); }
    }

    /// Equality is based on the hash and the words (numeric
    /// vector so not a heavy operation).
    auto operator==(InstanceKey const &that) const -> bool {
      return Hash == that.Hash and Words == that.Words;
    }

    /// An order over keys, so a set of them can be put in one
    /// canonical order within a compilation.
    auto operator<(InstanceKey const &that) const -> bool {
      return Words < that.Words;
    }

  private:
    /// The raw appending function, which takes the word, adds
    /// it into the vector, and updates the hash.
    auto Append(const Word word) -> void {
      Words.push_back(word);
      Hash = (Hash ^ word) * 0x9E3779B97F4A7C15ull;
      Hash ^= Hash >> 29;
    }
  };

  /// A key hashes as the hash it accumulated while it was built.
  /// No need to recalculate, and stable.
  SPP_EXP_CLS struct InstanceKeyHash {
    auto operator()(InstanceKey const &key) const noexcept -> std::uint64_t { return key.Hash; }
  };

  /// A "type id" is a pointer to an instance key, representing
  /// an interned type. They are the same id for the same type,
  /// meaningful within one compilation.
  SPP_EXP_CLS using TypeId = InstanceKey const*;

  /// A "comp id" is a pointer to a comp node ("CompNode"),
  /// representing an interned comp value, as a "type id" is a
  /// pointer to an interned key: the same id for the same value,
  /// meaningful within one compilation.
  SPP_EXP_CLS using CompId = CompNode const*;

  /// Convert a "type id" into a word, just by casting the
  /// address into a number. It is unique, so fine.
  SPP_EXP_FUN inline auto TypeIdWord(TypeId id) -> Word {
    return reinterpret_cast<std::uintptr_t>(id);
  }

  /// Convert a "word" into a "type id", by inverting the
  /// cast, reinterpreting it as the "type id" type.
  SPP_EXP_FUN inline auto TypeIdOfWord(const Word word) -> TypeId {
    return reinterpret_cast<TypeId>(static_cast<std::uintptr_t>(word));
  }

  /// Convert a "comp id" into a word, just by casting the
  /// address into a number, as for a "type id".
  SPP_EXP_FUN inline auto CompIdWord(const CompId id) -> Word {
    return reinterpret_cast<std::uintptr_t>(id);
  }

  /// Convert a "word" into a "comp id", by inverting the cast.
  SPP_EXP_FUN inline auto CompIdOfWord(const Word word) -> CompId {
    return reinterpret_cast<CompId>(static_cast<std::uintptr_t>(word));
  }

  /// Produce a "type id" from an "instance key".
  SPP_EXP_FUN SPP_ATTR_HOT inline auto InternTypeKey(InstanceKey &&key) -> TypeId {
    // Most keys are already interned, and an insert would move the key in before finding that out, so find first.
    static auto interned = StableSet<InstanceKey, InstanceKeyHash>();
    if (const auto hit = interned.find(key); hit != interned.end()) { return &*hit; }
    return &*interned.emplace(std::move(key)).first;
  }

  /// An order over keys that reads no address where anything else tells two keys apart: a symbol by its name and then
  /// the order it was made in ("TypeSymbol::Serial"), a nested type or comp identity by this same order, a spelling
  /// by its text, anything else by its value. Only keys alike in all of that fall back to their raw words, which keeps
  /// the order total. A variant's members are put in this order ("VariantKey"), so its tags do not follow where
  /// things happened to be allocated, and nothing it reads changes once made, so one set of members has one order.
  SPP_EXP_FUN auto StableKeyLess(InstanceKey const &lhs, InstanceKey const &rhs) -> bool;

  /// Given a generic parameter's id (on the symbol), get the
  /// type id being represented by it.
  SPP_EXP_FUN auto ParamTypeId(std::uint64_t param_id) -> TypeId;

  /// Given a generic parameter's id (on the symbol), get the
  /// comp id being represented by it.
  SPP_EXP_FUN auto ParamCompId(std::uint64_t param_id) -> CompId;

  /// The identity of the instantiation of "tmpl" whose arguments
  /// have the identity "args" ("Scope::ArgsIdOf"): an "Inst"
  /// head, then the arguments as one part.
  SPP_EXP_FUN inline auto InstanceIdOfArgs(TypeSymbol const &tmpl, const TypeId args) -> TypeId {
    auto key = InstanceKey();
    key.Push(InstanceKey::Tag::Inst);
    key.PushPtr(&tmpl);
    key.PushKey(*args);
    return InternTypeKey(std::move(key));
  }

  /// Append "part" to "into" as one part: by its "TypeId" when it is a type, whole when it has an unresolved part
  /// (which is no type, and has to keep marking "into" as none).
  SPP_EXP_FUN inline auto PushTypePart(InstanceKey &into, InstanceKey &&part) -> void {
    if (part.HasUnresolved) { into.PushKey(part); }
    else { into.PushId(InternTypeKey(std::move(part))); }
  }

  /// "PushTypePart" for a comp value: append its identity to "key" as one part, carrying what the types it names
  /// constants through are ("Self", unresolved), as a type part would. A constant named through a closed type that
  /// cannot be read yet ("CompMembers::Find") leaves the key unresolved, so it is keyed again once it can be.
  SPP_EXP_FUN auto PushCompPart(InstanceKey &key, CompId id) -> void;

  /// What a substitution rewrites in a "TypeId" ("SubstituteTypeId") or a "CompId" ("SubstituteCompId"): type and comp
  /// parameters by "ParamId" (0 standing for "Self", which a key spells rather than identifies), each to what it is
  /// bound to.
  SPP_EXP_CLS struct GenericSubst {
    std::vector<std::pair<std::uint64_t, TypeId>> TypeParams;
    std::vector<std::pair<std::uint64_t, CompId>> CompParams;

    /// The substituted parameters that are packs, of either kind (as "TypeParams" and "CompParams" name them): one
    /// standing as an element among others ("Tup[F, R]", "(f, r)") is spread into the elements it is bound to.
    std::vector<std::uint64_t> TypePackParams;
    std::vector<std::uint64_t> CompPackParams;

    SPP_ATTR_NODISCARD auto IsEmpty() const -> bool { return TypeParams.empty() and CompParams.empty(); }
  };

  /// How an expression carried out of the scope that wrote it is read ("ExpressionAst::ReadExpr",
  /// "type_resolution::ReadType" / "ReadComp"): each type and comp parameter it names is keyed in "Written", where it
  /// is the parameter it names, rewritten with "Bindings" ("Self" is parameter 0), and read back in "Reading". A
  /// default read in an instantiation's own scope, whose bindings are registered there, needs no "Bindings": it is
  /// written and read in that scope.
  SPP_EXP_CLS struct ExprSubst {
    GenericSubst Bindings;
    Scope const *Written = nullptr;
    Scope const *Reading = nullptr;

    /// Written and read in one scope (an instantiation's, whose bindings are registered there), with "bindings" applied
    /// on top.
    static auto In(Scope const &scope, GenericSubst bindings = {}) -> ExprSubst;

    /// Written in "written" (in the terms of the parameters "bindings" binds), read in "reading".
    static auto Across(Scope const &written, GenericSubst bindings, Scope const &reading) -> ExprSubst;
  };

  /// "id" with "subst" applied, rebuilt as keying the substituted type would build it: an instantiation's arguments
  /// rewritten in place, a variant's members re-flattened, deduplicated and put back in order, and a convention kept
  /// over the one a substituted type carries, and a comp argument rewritten at any depth ("SubstituteCompId"). Memoised
  /// per ("id", "subst"). Null when the result is no type (a part did not resolve).
  SPP_EXP_FUN SPP_ATTR_HOT auto SubstituteTypeId(TypeId id, GenericSubst const &subst) -> TypeId;

  /// "SubstituteTypeId" for a comp value's identity: "subst"'s comp bindings applied ("scopes::RewriteCompKey") at
  /// any depth, a pack spread, an operation folded once its operands are values, and a constant named through a type
  /// read once the type is closed. Zero when the result is no identity.
  SPP_EXP_FUN auto SubstituteCompId(CompId id, GenericSubst const &subst) -> CompId;

  /// How a constant named through a type is read, installed at compiler boot ("CompilerBoot::Stage1_PreProcess") by
  /// the layer that can look it up ("comp_generics::FindCompMemberId"); read only through "CompMemberIdOf".
  SPP_EXP_CLS struct CompMembers {
    inline static CompId (*Find)(TypeId owner, StrView name) = nullptr;
  };

  /// A constant named through a closed type ("Buf::n"), by identity ("CompMembers::Find"): the identity it is
  /// canonically - the value it folds to, else the member itself - or zero while it cannot be reached yet.
  SPP_EXP_FUN auto CompMemberIdOf(TypeId owner, StrView name) -> CompId;

  /// The outermost part of a "TypeId": what kind of type it is and what it is headed by.
  SPP_EXP_CLS struct TypeIdHead {
    /// "Self", "TypeParam", "TypeBound", "Symbol" (a closed class), "Inst", "Variant" or "Unresolved".
    InstanceKey::Tag Kind = InstanceKey::Tag::Unresolved;

    /// The convention tag, 0 for none.
    std::uint64_t Conv = 0;

    /// "Symbol": the class; "Inst": the template; "Variant": the variant template.
    void const *Ptr = nullptr;

    /// "TypeParam" and "TypeBound": the type parameter's "ParamId".
    std::uint64_t TypeParamId = 0;

    /// "Inst": the arguments' key, interned as the template's "Instances" files it.
    TypeId Args = nullptr;

    /// "Variant": the members, in their canonical order.
    std::vector<TypeId> Members;

    /// "Ptr" as the symbol it is: the class ("Symbol"), the template ("Inst") or the variant template ("Variant"). Null
    /// for any other kind.
    SPP_ATTR_NODISCARD auto Symbol() const -> TypeSymbol*;

    /// Whether this heads an instantiation: of a class template ("Inst"), or of the variant template ("Variant").
    SPP_ATTR_NODISCARD auto IsInstance() const -> bool;
  };

  /// Read the outermost part of "id" ("TypeIdHead"). Memoised.
  SPP_EXP_FUN SPP_ATTR_HOT auto HeadOf(TypeId id) -> TypeIdHead const&;

  /// "id" without its convention: the type a "TypeRef" holds under
  /// its own "Conv". Memoised.
  SPP_EXP_FUN SPP_ATTR_HOT auto BareTypeId(TypeId id) -> TypeId;

  /// The type parameters ("ParamId"s) "id" names anywhere, nested
  /// arguments and variant members included, and the comp
  /// parameters its comp arguments name (as "C<ParamId>" text ids).
  SPP_EXP_CLS struct TypeIdParams {
    std::vector<std::uint64_t> TypeParams;
    std::vector<std::uint64_t> CompParams;
  };

  /// "TypeIdParams" for "id". Memoised.
  SPP_EXP_FUN SPP_ATTR_HOT auto ParamsOf(TypeId id) -> TypeIdParams const&;

  /// Whether "id" names a type or comp parameter anywhere in it ("ParamsOf").
  SPP_EXP_FUN SPP_ATTR_HOT auto DoesTypeIdNameParams(TypeId id) -> bool;

  /// Whether "id" names a comp parameter anywhere in it, or a constant through a type that names one ("Box[T]::n"):
  /// such a value has none of its own until bound.
  SPP_EXP_FUN auto DoesCompIdNameParams(CompId id) -> bool;

  /// Whether "id" is a concrete type: it names no parameter and no "Self", so nothing a binding gives it changes it. A
  /// part that did not resolve is not counted against it ("IsClosedTypeId" does).
  SPP_EXP_FUN auto IsConcreteTypeId(TypeId id) -> bool;

  /// "IsConcreteTypeId" for a comp value: an identity naming no parameter. "Self" reaches one only as the owner of a
  /// constant named through it, which "DoesCompIdNameParams" counts.
  SPP_EXP_FUN auto IsConcreteCompId(CompId id) -> bool;

  /// Whether "id" is a type that means the same wherever it is read: concrete ("IsConcreteTypeId"), and with nothing
  /// unresolved.
  SPP_EXP_FUN auto IsClosedTypeId(TypeId id) -> bool;

  /// "IsClosedTypeId" for a comp value: concrete ("IsConcreteCompId"), and naming no constant through a type that
  /// cannot be read yet (an unresolved owner, or a closed one whose constant "CompMemberIdOf" cannot reach yet) - the
  /// part "PushCompPart" marks a key unresolved for.
  SPP_EXP_FUN auto IsClosedCompId(CompId id) -> bool;

  /// One argument of an instantiation's identity ("TypeIdHead::Args"): named (its interned name) or positional (its
  /// index), then a type or a comp value's interned identity text.
  SPP_EXP_CLS struct TypeIdArg {
    bool Named = false;
    std::uint64_t Name = 0;
    TypeId TypeVal = nullptr;
    CompId CompVal = nullptr;
  };

  /// The arguments an instantiation's arguments key lists, in order.
  SPP_EXP_FUN auto ArgsOf(TypeId args) -> std::vector<TypeIdArg>;
}
