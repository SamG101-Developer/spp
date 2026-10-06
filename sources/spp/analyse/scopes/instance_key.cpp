module;
#include <spp/macros.hpp>

module spp.analyse.scopes.instance_key;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.symbols;
import spp.asts.type_identifier_ast;
import spp.utils.interner;
import spp.utils.types;
import genex;
import std;

namespace spp::analyse::scopes {
  namespace {
    using Tag = InstanceKey::Tag;
    using Words = std::span<std::uint64_t const>;

    auto TagOf(const std::uint64_t word) -> Tag {
      return static_cast<Tag>(word >> 56);
    }

    auto PayloadOf(const std::uint64_t word) -> std::uint64_t {
      return word & ((std::uint64_t{1} << 56) - 1);
    }

    /// Whether a constant named through the type "type" (a "TypeId"'s word) cannot be read yet: its type did not
    /// resolve, or is closed and its constant cannot be reached yet ("CompMemberIdOf"). An open type's constant is read
    /// once a substitution closes it.
    auto MemberUnresolved(const std::uint64_t type, const StrView name) -> bool {
      auto const *const owner = TypeIdOfWord(type);
      return owner->HasUnresolved or (IsClosedTypeId(owner) and CompMemberIdOf(owner, name) == nullptr);
    }

    auto WordsOf(const TypeId id) -> Words {
      return {id->Words.data(), id->Words.size()};
    }

    auto StableCompare(Words lhs, Words rhs) -> std::strong_ordering;

    /// "StableKeyLess" for two interned types, and for two type words.
    auto StableCompareIds(const TypeId lhs, const TypeId rhs) -> std::strong_ordering {
      return lhs == rhs ? std::strong_ordering::equal : StableCompare(WordsOf(lhs), WordsOf(rhs));
    }

    /// "StableKeyLess" for two comp identities: part by part, a value by its value and a constant's type by its own
    /// order. Interned, so two that are alike in all of that are one node, unless a type in them ties on its name.
    auto StableCompareNodes(CompNode const &lhs, CompNode const &rhs) -> std::strong_ordering {
      if (&lhs == &rhs) { return std::strong_ordering::equal; }
      if (const auto c = lhs.Kind <=> rhs.Kind; c != 0) { return c; }
      if (const auto c = lhs.Text <=> rhs.Text; c != 0) { return c; }
      if (const auto c = lhs.ParamId <=> rhs.ParamId; c != 0) { return c; }
      if (lhs.Type != rhs.Type) {
        if (const auto c = StableCompareIds(TypeIdOfWord(lhs.Type), TypeIdOfWord(rhs.Type)); c != 0) { return c; }
      }
      if (const auto c = lhs.Val.index() <=> rhs.Val.index(); c != 0) { return c; }
      if (auto const *const l = lhs.AsInt(); l != nullptr) {
        if (const auto c = *l <=> *rhs.AsInt(); c != 0) { return c; }
      }
      if (auto const *const l = lhs.AsFloat(); l != nullptr) {
        if (const auto c = *l <=> *rhs.AsFloat(); c != 0) { return c; }
      }
      if (auto const *const l = lhs.AsBool(); l != nullptr) {
        if (const auto c = *l <=> *rhs.AsBool(); c != 0) { return c; }
      }
      for (auto i = 0uz; i < lhs.Kids.size() and i < rhs.Kids.size(); ++i) {
        if (const auto c = StableCompareNodes(*lhs.Kids[i], *rhs.Kids[i]); c != 0) { return c; }
      }
      return lhs.Kids.size() <=> rhs.Kids.size();
    }

    /// Two symbol words by what is fixed when the symbols are made: the name, then which was made first. Not the
    /// qualified name, which changes when a scope is re-parented: a variant keyed before and after would split into
    /// two identities. Every pointer a key holds after "Symbol" is a type symbol's.
    auto StableCompareSymbols(const std::uint64_t lhs, const std::uint64_t rhs) -> std::strong_ordering {
      auto const *const l = reinterpret_cast<TypeSymbol const*>(static_cast<std::uintptr_t>(lhs));
      auto const *const r = reinterpret_cast<TypeSymbol const*>(static_cast<std::uintptr_t>(rhs));
      const auto l_name = l->Name != nullptr ? l->Name->ToView() : StrView();
      const auto r_name = r->Name != nullptr ? r->Name->ToView() : StrView();
      if (const auto c = l_name <=> r_name; c != 0) { return c; }
      return l->Serial <=> r->Serial;
    }

    auto StableCompare(const Words lhs, const Words rhs) -> std::strong_ordering {
      // The two keys are walked together while they are alike, so their raw words line up: a pointer follows only a
      // "Symbol", "TypeId" or "CompId" tag.
      for (auto i = 0uz; i < lhs.size() and i < rhs.size(); ++i) {
        const auto tag = TagOf(lhs[i]);
        if (lhs[i] != rhs[i]) {
          const auto spelled = tag == Tag::Name or tag == Tag::Unresolved or tag == Tag::TypeBound;
          if (spelled and TagOf(rhs[i]) == tag) {
            return WordText(PayloadOf(lhs[i])) <=> WordText(PayloadOf(rhs[i]));
          }
          return lhs[i] <=> rhs[i];
        }
        if (tag != Tag::Symbol and tag != Tag::TypeId and tag != Tag::CompId) { continue; }
        if (++i == lhs.size() or i == rhs.size() or lhs[i] == rhs[i]) { continue; }
        const auto c = tag == Tag::Symbol ? StableCompareSymbols(lhs[i], rhs[i])
          : tag == Tag::TypeId ? StableCompareIds(TypeIdOfWord(lhs[i]), TypeIdOfWord(rhs[i]))
          : StableCompareNodes(*CompIdOfWord(lhs[i]), *CompIdOfWord(rhs[i]));
        if (c != 0) { return c; }
      }

      // Alike in everything read so far: the shorter first, and then, as a last resort, the raw words.
      if (const auto c = lhs.size() <=> rhs.size(); c != 0) { return c; }
      return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    /// Whether raw key words hold a "Self": a pointer word's top byte is never a tag's.
    auto HasSelfWord(Words words) -> bool {
      return genex::any_of(words, [](const std::uint64_t w) { return TagOf(w) == Tag::Self; });
    }

    /// One type part at the front of "words" - by id, or inline after its length - as a "TypeId", and how many
    /// words it took.
    auto DecodePart(const Words words, std::size_t &taken) -> TypeId {
      if (TagOf(words[0]) == Tag::TypeId) {
        taken = 2;
        return TypeIdOfWord(words[1]);
      }
      const auto len = PayloadOf(words[0]);
      taken = 1 + len;
      auto key = InstanceKey();
      key.AppendWords(words.subspan(1, len), true, HasSelfWord(words.subspan(1, len)));
      return InternTypeKey(std::move(key));
    }

    struct MemoHash {
      auto operator()(Pair<TypeId, TypeId> const &p) const noexcept -> std::size_t {
        return std::hash<void const*>()(p.first) * 31 ^ std::hash<void const*>()(p.second);
      }
    };

    auto Memo() -> Map<Pair<TypeId, TypeId>, TypeId, MemoHash>& {
      static auto memo = Map<Pair<TypeId, TypeId>, TypeId, MemoHash>();
      return memo;
    }

    /// What "subst" binds type parameter "param" to, and comp parameter "param" to; none (null, zero) when it does not
    /// bind it.
    auto BoundTypeOf(GenericSubst const &subst, const std::uint64_t param) -> TypeId {
      const auto it = genex::find(subst.TypeParams, param, &std::pair<std::uint64_t, TypeId>::first);
      return it != subst.TypeParams.end() ? it->second : nullptr;
    }

    auto BoundCompOf(GenericSubst const &subst, const std::uint64_t param) -> CompId {
      const auto it = genex::find(subst.CompParams, param, &std::pair<std::uint64_t, CompId>::first);
      return it != subst.CompParams.end() ? it->second : nullptr;
    }

    class Rewriter {
    public:
      Rewriter(GenericSubst const &subst, const TypeId subst_id) : _Subst(subst), _SubstId(subst_id) {}

      /// "id" rewritten, interned whatever it holds; null when it cannot be.
      auto Part(TypeId id) -> TypeId {
        auto &memo = Memo();
        if (const auto hit = memo.find({id, _SubstId}); hit != memo.end()) { return hit->second; }
        auto out = InstanceKey();
        const auto result = RewriteType(WordsOf(id), out) ? InternTypeKey(std::move(out)) : nullptr;
        memo.emplace(Pair<TypeId, TypeId>{id, _SubstId}, result);
        return result;
      }

    private:
      GenericSubst const &_Subst;
      TypeId _SubstId;

      /// A bound type in place of a parameter, under the parameter's convention where it has one.
      static auto Splice(const TypeId bound, const std::uint64_t conv, InstanceKey &out) -> void {
        auto words = WordsOf(bound);
        if (conv != 0) {
          out.Push(Tag::Conv, conv);
          if (not words.empty() and TagOf(words[0]) == Tag::Conv) { words = words.subspan(1); }
        }
        out.AppendWords(words, bound->HasUnresolved, bound->HasSelf);
      }

      /// One whole type ("words" is exactly it) rewritten into "out".
      auto RewriteType(Words words, InstanceKey &out) -> bool {
        if (words.empty()) { return false; }
        auto conv = std::uint64_t{0};
        if (TagOf(words[0]) == Tag::Conv) {
          conv = PayloadOf(words[0]);
          words = words.subspan(1);
          if (words.empty()) { return false; }
        }

        switch (TagOf(words[0])) {
        case Tag::Self:
        case Tag::TypeParam:
        case Tag::TypeBound: {
          const auto param = TagOf(words[0]) == Tag::Self ? 0 : PayloadOf(words[0]);
          if (const auto bound = BoundTypeOf(_Subst, param); bound != nullptr) {
            Splice(bound, conv, out);
            return true;
          }
          if (conv != 0) { out.Push(Tag::Conv, conv); }
          out.AppendWords(words, false, TagOf(words[0]) == Tag::Self);
          return true;
        }
        case Tag::Unresolved:
        case Tag::Symbol: {
          if (conv != 0) { out.Push(Tag::Conv, conv); }
          out.AppendWords(words, TagOf(words[0]) == Tag::Unresolved);
          return true;
        }
        case Tag::Inst: {
          // "Inst", the template, then the arguments' key under its length.
          if (words.size() < 4 or TagOf(words[1]) != Tag::Symbol or TagOf(words[3]) != Tag::Len) { return false; }
          auto args = InstanceKey();
          if (not RewriteArgs(words.subspan(4, PayloadOf(words[3])), args)) { return false; }
          if (conv != 0) { out.Push(Tag::Conv, conv); }
          out.Push(Tag::Inst);
          out.PushPtr(reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[2])));
          out.PushKey(*InternTypeKey(std::move(args)));
          return true;
        }
        case Tag::Variant: {
          // "Variant", the template, then its members: flattened, deduplicated and ordered as "VariantKey" does.
          if (words.size() < 3 or TagOf(words[1]) != Tag::Symbol) { return false; }
          auto members = std::vector<TypeId>();
          for (auto rest = words.subspan(3); not rest.empty();) {
            auto taken = std::size_t{0};
            const auto member = Part(DecodePart(rest, taken));
            rest = rest.subspan(taken);
            if (member == nullptr) { return false; }
            auto const &head = HeadOf(member);
            auto flat = head.Kind == Tag::Variant and head.Conv == 0 ? head.Members : std::vector{member};
            for (const auto m : flat) {
              if (not genex::contains(members, m)) { members.push_back(m); }
            }
          }
          members |= genex::actions::sort([](const TypeId a, const TypeId b) { return StableKeyLess(*a, *b); });
          if (conv != 0) { out.Push(Tag::Conv, conv); }
          out.Push(Tag::Variant);
          out.PushPtr(reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[2])));
          for (const auto m : members) { PushTypePart(out, InstanceKey(*m)); }
          return true;
        }
        default:
          return false;
        }
      }

      /// An instantiation's arguments ("Scope::ArgsIdOf"): each a name or position, then a type part or a
      /// comp identity.
      auto RewriteArgs(Words words, InstanceKey &out) -> bool {
        // Positions after a spread pack move along by the elements it added.
        auto shift = std::uint64_t{0};
        while (not words.empty()) {
          const auto tag = TagOf(words[0]);
          if (tag != Tag::Name and tag != Tag::Pos) { return false; }
          const auto name = PayloadOf(words[0]);
          const auto pos = name + shift;
          words = words.subspan(1);
          if (words.empty()) { return false; }

          if (TagOf(words[0]) == Tag::CompId) {
            if (words.size() < 2) { return false; }
            out.Push(tag, tag == Tag::Pos ? pos : name);
            if (not RewriteComp(CompIdOfWord(words[1]), out)) { return false; }
            words = words.subspan(2);
            continue;
          }
          auto taken = std::size_t{0};
          const auto written = DecodePart(words, taken);
          words = words.subspan(taken);
          if (tag == Tag::Pos) {
            if (const auto elems = SpreadTypePack(written); elems.has_value()) {
              for (auto i = 0uz; i < elems->size(); ++i) {
                out.Push(Tag::Pos, pos + i);
                PushTypePart(out, InstanceKey(*(*elems)[i]));
              }
              shift += elems->size() - 1;
              continue;
            }
          }
          out.Push(tag, tag == Tag::Pos ? pos : name);
          const auto part = Part(written);
          if (part == nullptr) { return false; }
          PushTypePart(out, InstanceKey(*part));
        }
        return true;
      }

      /// The elements a type pack standing as one element is spread into: the arguments of the tuple it is bound to.
      auto SpreadTypePack(TypeId written) const -> std::optional<std::vector<TypeId>> {
        auto const &head = HeadOf(written);
        if (head.Kind != Tag::TypeParam or head.Conv != 0
          or not genex::contains(_Subst.TypePackParams, head.TypeParamId)) {
          return std::nullopt;
        }
        // A pack is bound to a tuple: the bare template when it is empty.
        const auto bound = BoundTypeOf(_Subst, head.TypeParamId);
        if (bound == nullptr) { return std::nullopt; }
        auto elems = std::vector<TypeId>();
        if (HeadOf(bound).Kind == Tag::Symbol) { return elems; }
        if (HeadOf(bound).Kind != Tag::Inst) { return std::nullopt; }
        for (auto const &arg : ArgsOf(HeadOf(bound).Args)) {
          if (arg.Named or arg.TypeVal == nullptr) { return std::nullopt; }
          elems.push_back(arg.TypeVal);
        }
        return elems;
      }

      /// One comp argument's identity rewritten into "out" ("SubstituteCompId"): each substituted parameter at any
      /// depth its value, a pack parameter among a pack's elements spread, and an operation folded once its operands
      /// are values.
      auto RewriteComp(const CompId value, InstanceKey &out) -> bool {
        const auto rewritten = SubstituteCompId(value, _Subst);
        if (rewritten == nullptr) { return false; }
        PushCompPart(out, rewritten);
        return true;
      }
    };

    /// "subst" as one interned key, so a memo entry is a pair of pointers.
    auto SubstId(GenericSubst const &subst) -> TypeId {
      auto types = subst.TypeParams;
      auto comps = subst.CompParams;
      types |= genex::actions::sort;
      comps |= genex::actions::sort;
      auto key = InstanceKey();
      for (auto const &[param, type] : types) {
        key.Push(Tag::TypeParam, param);
        key.PushId(type);
      }
      for (auto const &[param, value] : comps) {
        key.Push(Tag::CompParam, param);
        key.PushCompId(value);
      }
      for (const auto pack : subst.TypePackParams) { key.Push(Tag::TypePack, pack); }
      for (const auto pack : subst.CompPackParams) { key.Push(Tag::CompPack, pack); }
      return InternTypeKey(std::move(key));
    }
  }
}

auto spp::analyse::scopes::PushCompPart(
  InstanceKey &key,
  const CompId id)
  -> void {
  key.PushCompId(id);
  if (id == nullptr) { return; }
  for (auto const &[type, name] : CompKeyMembers(*id)) {
    key.HasSelf = key.HasSelf or TypeIdOfWord(type)->HasSelf;
    key.HasUnresolved = key.HasUnresolved or MemberUnresolved(type, name);
  }
}

auto spp::analyse::scopes::SubstituteCompId(
  const CompId id,
  GenericSubst const &subst)
  -> CompId {
  if (id == nullptr) { return nullptr; }
  const auto bound = [&subst](const std::uint64_t param_id) -> std::optional<CompNode> {
    const auto value = BoundCompOf(subst, param_id);
    return value != nullptr ? std::optional(*value) : std::nullopt;
  };
  const auto is_pack = [&subst](const std::uint64_t param_id) {
    return genex::contains(subst.CompPackParams, param_id);
  };
  // A constant named through a type: the type rewritten, and read once it is closed ("CompMembers::Find").
  const auto member = [&subst](const std::uint64_t word, const StrView name) -> std::optional<CompNode> {
    const auto owner = SubstituteTypeId(TypeIdOfWord(word), subst);
    if (owner == nullptr) { return std::nullopt; }
    if (IsClosedTypeId(owner)) {
      if (const auto read = CompMemberIdOf(owner, name); read != nullptr) { return *read; }
    }
    return CompNode::OfMember(TypeIdWord(owner), name);
  };
  const auto rewritten = RewriteCompKey(*id, bound, is_pack, member);
  return rewritten.has_value() ? InternCompKey(*rewritten) : nullptr;
}

auto spp::analyse::scopes::SubstituteTypeId(
  const TypeId id,
  GenericSubst const &subst)
  -> TypeId {
  if (id == nullptr) { return nullptr; }
  if (subst.IsEmpty()) { return id->HasUnresolved ? nullptr : id; }
  auto rewriter = Rewriter(subst, SubstId(subst));
  const auto result = rewriter.Part(id);
  return result == nullptr or result->HasUnresolved ? nullptr : result;
}

auto spp::analyse::scopes::HeadOf(
  const TypeId id)
  -> TypeIdHead const& {
  // Keys and their parts are interned for the whole process, so the answer is kept for as long, and on the key.
  static auto memo = StableMap<TypeId, TypeIdHead>();
  if (id != nullptr and id->CachedHead != nullptr) { return *id->CachedHead; }
  const auto remember = [id](TypeIdHead &&answer) -> TypeIdHead const& {
    auto const &kept = memo.emplace(id, std::move(answer)).first->second;
    if (id != nullptr) { id->CachedHead = &kept; }
    return kept;
  };
  auto head = TypeIdHead();
  auto words = WordsOf(id);
  if (not words.empty() and TagOf(words[0]) == Tag::Conv) {
    head.Conv = PayloadOf(words[0]);
    words = words.subspan(1);
  }
  if (words.empty()) { return remember(std::move(head)); }

  head.Kind = TagOf(words[0]);
  switch (head.Kind) {
  case Tag::TypeParam:
  case Tag::TypeBound:
    head.TypeParamId = PayloadOf(words[0]);
    break;
  case Tag::Symbol:
    if (words.size() >= 2) { head.Ptr = reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[1])); }
    break;
  case Tag::Inst:
    if (words.size() >= 4 and TagOf(words[3]) == Tag::Len) {
      head.Ptr = reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[2]));
      auto args = InstanceKey();
      args.AppendWords(words.subspan(4, PayloadOf(words[3])), id->HasUnresolved, id->HasSelf);
      head.Args = InternTypeKey(std::move(args));
    }
    break;
  case Tag::Variant:
    if (words.size() >= 3) {
      head.Ptr = reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[2]));
      for (auto rest = words.subspan(3); not rest.empty();) {
        auto taken = std::size_t{0};
        head.Members.push_back(DecodePart(rest, taken));
        rest = rest.subspan(taken);
      }
    }
    break;
  default:
    break;
  }
  return remember(std::move(head));
}

auto spp::analyse::scopes::BareTypeId(
  const TypeId id)
  -> TypeId {
  if (id == nullptr or id->Words.empty() or TagOf(id->Words[0]) != Tag::Conv) { return id; }
  // Keys and their parts are interned for the whole process, so the answer is kept for as long, on the key.
  if (id->CachedBare != nullptr) { return id->CachedBare; }
  auto key = InstanceKey();
  key.AppendWords(WordsOf(id).subspan(1), id->HasUnresolved, id->HasSelf);
  id->CachedBare = InternTypeKey(std::move(key));
  return id->CachedBare;
}

auto spp::analyse::scopes::ParamsOf(
  const TypeId id)
  -> TypeIdParams const& {
  // Keys and their parts are interned for the whole process, so the answer is kept for as long, and on the key.
  static auto memo = StableMap<TypeId, TypeIdParams>();
  if (id != nullptr and id->CachedParams != nullptr) { return *id->CachedParams; }
  if (const auto hit = memo.find(id); hit != memo.end()) { return hit->second; }
  auto out = TypeIdParams();
  const auto add = [](std::vector<std::uint64_t> &into, const std::uint64_t x) {
    if (not genex::contains(into, x)) { into.push_back(x); }
  };
  const auto words = WordsOf(id);
  for (auto i = 0uz; i < words.size(); ++i) {
    switch (TagOf(words[i])) {
    case Tag::TypeParam:
      add(out.TypeParams, PayloadOf(words[i]));
      break;
    case Tag::TypeBound:
      add(out.TypeParams, PayloadOf(words[i]));
      ++i;
      break;
    case Tag::Symbol:
      ++i;
      break;
    case Tag::TypeId: {
      auto const &inner = ParamsOf(TypeIdOfWord(words[i + 1]));
      for (const auto t : inner.TypeParams) { add(out.TypeParams, t); }
      for (const auto c : inner.CompParams) { add(out.CompParams, c); }
      ++i;
      break;
    }
    case Tag::CompId: {
      // A comp identity names its parameters as "Param" parts ("scopes::CompKeyParams"), and the types it names
      // constants through by their ids, whose parameters are its own too. Its address is the next word.
      const auto node = CompIdOfWord(words[i + 1]);
      ++i;
      if (node == nullptr) { break; }
      for (const auto param_id : CompKeyParams(*node)) { add(out.CompParams, param_id); }
      for (auto const &[type, _] : CompKeyMembers(*node)) {
        auto const &inner = ParamsOf(TypeIdOfWord(type));
        for (const auto t : inner.TypeParams) { add(out.TypeParams, t); }
        for (const auto comp : inner.CompParams) { add(out.CompParams, comp); }
      }
      break;
    }
    default:
      break;
    }
  }
  auto const &kept = memo.emplace(id, std::move(out)).first->second;
  if (id != nullptr) { id->CachedParams = &kept; }
  return kept;
}

auto spp::analyse::scopes::DoesTypeIdNameParams(
  const TypeId id)
  -> bool {
  auto const &params = ParamsOf(id);
  return not params.TypeParams.empty() or not params.CompParams.empty();
}

auto spp::analyse::scopes::DoesCompIdNameParams(
  const CompId id)
  -> bool {
  return id != nullptr and id->Any([](CompNode const &part) {
    return part.Kind == CompNode::Part::Param
      or (part.Kind == CompNode::Part::Member and not IsClosedTypeId(TypeIdOfWord(part.Type)));
  });
}

auto spp::analyse::scopes::CompMemberIdOf(
  const TypeId owner,
  const StrView name)
  -> CompId {
  return CompMembers::Find != nullptr ? CompMembers::Find(owner, name) : nullptr;
}

auto spp::analyse::scopes::StableKeyLess(
  InstanceKey const &lhs,
  InstanceKey const &rhs)
  -> bool {
  return StableCompare({lhs.Words.data(), lhs.Words.size()}, {rhs.Words.data(), rhs.Words.size()}) < 0;
}

auto spp::analyse::scopes::ParamTypeId(
  const std::uint64_t param_id)
  -> TypeId {
  auto key = InstanceKey();
  key.Push(Tag::TypeParam, param_id);
  return InternTypeKey(std::move(key));
}

auto spp::analyse::scopes::ParamCompId(
  const std::uint64_t param_id)
  -> CompId {
  return InternCompKey(CompNode::OfParam(param_id));
}

auto spp::analyse::scopes::IsConcreteTypeId(
  const TypeId id)
  -> bool {
  return id != nullptr and not id->HasSelf and not DoesTypeIdNameParams(id);
}

auto spp::analyse::scopes::IsConcreteCompId(
  const CompId id)
  -> bool {
  return id != nullptr and not DoesCompIdNameParams(id);
}

auto spp::analyse::scopes::IsClosedTypeId(
  const TypeId id)
  -> bool {
  return IsConcreteTypeId(id) and not id->HasUnresolved;
}

auto spp::analyse::scopes::IsClosedCompId(
  const CompId id)
  -> bool {
  if (not IsConcreteCompId(id)) { return false; }
  return not id->Any([](CompNode const &part) {
    return part.Kind == CompNode::Part::Member and MemberUnresolved(part.Type, part.Text);
  });
}

auto spp::analyse::scopes::ArgsOf(
  const TypeId args)
  -> std::vector<TypeIdArg> {
  auto out = std::vector<TypeIdArg>();
  if (args == nullptr) { return out; }
  auto words = WordsOf(args);
  while (not words.empty()) {
    auto arg = TypeIdArg();
    arg.Named = TagOf(words[0]) == Tag::Name;
    arg.Name = PayloadOf(words[0]);
    words = words.subspan(1);
    if (words.empty()) { break; }
    if (TagOf(words[0]) == Tag::CompId) {
      if (words.size() < 2) { break; }
      arg.CompVal = CompIdOfWord(words[1]);
      words = words.subspan(2);
    }
    else {
      auto taken = std::size_t{0};
      arg.TypeVal = DecodePart(words, taken);
      words = words.subspan(taken);
    }
    out.push_back(arg);
  }
  return out;
}

SPP_MOD_BEGIN
auto spp::analyse::scopes::TypeIdHead::Symbol() const -> TypeSymbol* {
  const auto names_sym = Kind == InstanceKey::Tag::Symbol or IsInstance();
  return names_sym ? const_cast<TypeSymbol*>(static_cast<TypeSymbol const*>(Ptr)) : nullptr;
}

auto spp::analyse::scopes::TypeIdHead::IsInstance() const -> bool {
  return Kind == InstanceKey::Tag::Inst or Kind == InstanceKey::Tag::Variant;
}

auto spp::analyse::scopes::ExprSubst::In(
  Scope const &scope, GenericSubst bindings) -> ExprSubst {
  return {.Bindings = std::move(bindings), .Written = &scope, .Reading = &scope};
}

auto spp::analyse::scopes::ExprSubst::Across(
  Scope const &written, GenericSubst bindings, Scope const &reading) -> ExprSubst {
  return {.Bindings = std::move(bindings), .Written = &written, .Reading = &reading};
}
SPP_MOD_END
