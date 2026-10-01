module;
#include <spp/macros.hpp>

module spp.analyse.scopes.instance_key;
import spp.utils.interner;
import spp.utils.types;
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

    auto AsId(const std::uint64_t word) -> TypeId {
      return reinterpret_cast<TypeId>(static_cast<std::uintptr_t>(word));
    }

    auto WordsOf(TypeId id) -> Words {
      return {id->Words.data(), id->Words.size()};
    }

    /// Whether raw key words hold a "Self": a pointer word's top byte is never a tag's.
    auto HasSelfWord(Words words) -> bool {
      return std::ranges::any_of(words, [](const std::uint64_t w) { return TagOf(w) == Tag::Self; });
    }

    /// Intern a key whatever it holds: an unresolved part is still one part of another key.
    auto InternPart(InstanceKey &&key) -> TypeId {
      return InternTypeKey(std::move(key));
    }

    /// One type part at the front of "words" - by id, or inline after its length - as a "TypeId", and how many
    /// words it took.
    auto ReadPart(Words words, std::size_t &taken) -> TypeId {
      if (TagOf(words[0]) == Tag::Id) {
        taken = 2;
        return AsId(words[1]);
      }
      const auto len = PayloadOf(words[0]);
      taken = 1 + len;
      auto key = InstanceKey();
      key.AppendWords(words.subspan(1, len), true, HasSelfWord(words.subspan(1, len)));
      return InternPart(std::move(key));
    }

    struct MemoHash {
      auto operator()(Pair<TypeId, TypeId> const &p) const noexcept -> std::size_t {
        return std::hash<void const*>()(p.first) * 31 ^ std::hash<void const*>()(p.second);
      }
    };

    auto Memo() -> std::unordered_map<Pair<TypeId, TypeId>, TypeId, MemoHash>& {
      static auto memo = std::unordered_map<Pair<TypeId, TypeId>, TypeId, MemoHash>();
      return memo;
    }

    class Rewriter {
    public:
      Rewriter(TypeSubst const &subst, TypeId subst_id) : _Subst(subst), _SubstId(subst_id) {
        for (auto const &[param, _] : subst.Comps) {
          _CompNames.emplace_back(utils::InternedText(static_cast<utils::InternedId>(param)));
        }
      }

      /// "id" rewritten, interned whatever it holds; null when it cannot be.
      auto Part(TypeId id) -> TypeId {
        auto &memo = Memo();
        if (const auto hit = memo.find({id, _SubstId}); hit != memo.end()) { return hit->second; }
        auto out = InstanceKey();
        const auto result = Type(WordsOf(id), out) ? InternPart(std::move(out)) : nullptr;
        memo.emplace(Pair<TypeId, TypeId>{id, _SubstId}, result);
        return result;
      }

    private:
      TypeSubst const &_Subst;
      TypeId _SubstId;
      std::vector<StrView> _CompNames;

      auto BoundType(const std::uint64_t param) const -> TypeId {
        for (auto const &[p, t] : _Subst.Types) { if (p == param) { return t; } }
        return nullptr;
      }

      /// A bound type in place of a parameter, under the parameter's convention where it has one.
      static auto Splice(TypeId bound, const std::uint64_t conv, InstanceKey &out) -> void {
        auto words = WordsOf(bound);
        if (conv != 0) {
          out.Push(Tag::Conv, conv);
          if (not words.empty() and TagOf(words[0]) == Tag::Conv) { words = words.subspan(1); }
        }
        out.AppendWords(words, bound->HasUnresolved, bound->HasSelf);
      }

      /// One whole type ("words" is exactly it) rewritten into "out".
      auto Type(Words words, InstanceKey &out) -> bool {
        if (words.empty()) { return false; }
        auto conv = std::uint64_t{0};
        if (TagOf(words[0]) == Tag::Conv) {
          conv = PayloadOf(words[0]);
          words = words.subspan(1);
          if (words.empty()) { return false; }
        }

        switch (TagOf(words[0])) {
        case Tag::Self:
        case Tag::Param:
        case Tag::Bound: {
          const auto param = TagOf(words[0]) == Tag::Self ? 0 : PayloadOf(words[0]);
          if (const auto bound = BoundType(param); bound != nullptr) {
            Splice(bound, conv, out);
            return true;
          }
          if (conv != 0) { out.Push(Tag::Conv, conv); }
          out.AppendWords(words, false, TagOf(words[0]) == Tag::Self);
          return true;
        }
        case Tag::Unresolved:
        case Tag::Sym: {
          if (conv != 0) { out.Push(Tag::Conv, conv); }
          out.AppendWords(words, TagOf(words[0]) == Tag::Unresolved);
          return true;
        }
        case Tag::Inst: {
          // "Inst", the template, then the arguments' key under its length.
          if (words.size() < 4 or TagOf(words[1]) != Tag::Sym or TagOf(words[3]) != Tag::Len) { return false; }
          auto args = InstanceKey();
          if (not Args(words.subspan(4, PayloadOf(words[3])), args)) { return false; }
          if (conv != 0) { out.Push(Tag::Conv, conv); }
          out.Push(Tag::Inst);
          out.PushPtr(reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[2])));
          out.PushKey(*InternTypeKey(std::move(args)));
          return true;
        }
        case Tag::Variant: {
          // "Variant", the template, then its members: flattened, deduplicated and ordered as "variant_key" does.
          if (words.size() < 3 or TagOf(words[1]) != Tag::Sym) { return false; }
          auto members = std::vector<TypeId>();
          for (auto rest = words.subspan(3); not rest.empty();) {
            auto taken = std::size_t{0};
            const auto member = Part(ReadPart(rest, taken));
            rest = rest.subspan(taken);
            if (member == nullptr) { return false; }
            auto const &head = HeadOf(member);
            auto flat = head.Kind == Tag::Variant and head.Conv == 0 ? head.Members : std::vector{member};
            for (const auto m : flat) {
              if (std::ranges::find(members, m) == members.end()) { members.push_back(m); }
            }
          }
          std::ranges::sort(members, [](TypeId a, TypeId b) { return *a < *b; });
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

      /// An instantiation's arguments ("Scope::InstanceIdentityKey"): each a name or position, then a type part or a
      /// comp identity.
      auto Args(Words words, InstanceKey &out) -> bool {
        // Positions after a spread pack move along by the elements it added.
        auto shift = std::uint64_t{0};
        while (not words.empty()) {
          const auto tag = TagOf(words[0]);
          if (tag != Tag::Name and tag != Tag::Pos) { return false; }
          const auto name = PayloadOf(words[0]);
          const auto pos = name + shift;
          words = words.subspan(1);
          if (words.empty()) { return false; }

          if (TagOf(words[0]) == Tag::Comp) {
            out.Push(tag, tag == Tag::Pos ? pos : name);
            if (not Comp(PayloadOf(words[0]), out)) { return false; }
            words = words.subspan(1);
            continue;
          }
          auto taken = std::size_t{0};
          const auto written = ReadPart(words, taken);
          words = words.subspan(taken);
          if (tag == Tag::Pos) {
            if (const auto elems = SpreadPack(written); elems.has_value()) {
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
      auto SpreadPack(TypeId written) const -> std::optional<std::vector<TypeId>> {
        auto const &head = HeadOf(written);
        if (head.Kind != Tag::Param or head.Conv != 0 or not std::ranges::contains(_Subst.TypePacks, head.ParamId)) {
          return std::nullopt;
        }
        // A pack is bound to a tuple: the bare template when it is empty.
        const auto bound = BoundType(head.ParamId);
        if (bound == nullptr) { return std::nullopt; }
        auto elems = std::vector<TypeId>();
        if (HeadOf(bound).Kind == Tag::Sym) { return elems; }
        if (HeadOf(bound).Kind != Tag::Inst) { return std::nullopt; }
        for (auto const &arg : ArgsOf(HeadOf(bound).Args)) {
          if (arg.Named or arg.Type == nullptr) { return std::nullopt; }
          elems.push_back(arg.Type);
        }
        return elems;
      }

      /// A comp argument's identity: a parameter bound here is its value's; an operation naming one cannot be
      /// rewritten without folding it. A pack's elements are each rewritten as a whole value would be.
      auto Comp(const std::uint64_t text, InstanceKey &out) -> bool {
        auto rewritten = std::uint64_t{0};
        if (not CompText(text, rewritten)) { return false; }
        out.Push(Tag::Comp, rewritten);
        return true;
      }

      auto CompText(const std::uint64_t text, std::uint64_t &out) -> bool {
        for (auto const &[param, value] : _Subst.Comps) {
          if (param == text) {
            out = value;
            return true;
          }
        }
        const auto spelled = utils::InternedText(static_cast<utils::InternedId>(text));
        if (const auto elems = CompPackElements(spelled); elems.has_value()) {
          auto pack = Str("P(");
          auto first = true;
          for (auto const written : *elems) {
            const auto written_id = static_cast<std::uint64_t>(utils::Intern(written));
            auto elem = std::uint64_t{0};
            if (not CompText(written_id, elem)) { return false; }
            // A comp pack standing as one element is spread into the elements it is bound to, as a type pack is.
            const auto elem_text = utils::InternedText(static_cast<utils::InternedId>(elem));
            const auto spread = std::ranges::contains(_Subst.CompPacks, written_id) ? CompPackElements(elem_text) : std::nullopt;
            for (auto const part : spread.has_value() ? *spread : std::vector{elem_text}) {
              if (not first) { pack += ", "; }
              pack += part;
              first = false;
            }
          }
          pack += ')';
          out = static_cast<std::uint64_t>(utils::Intern(pack));
          return true;
        }
        for (auto const &name : _CompNames) {
          for (auto at = spelled.find(name); at != StrView::npos; at = spelled.find(name, at + 1)) {
            if (at != 0 and std::isalnum(static_cast<unsigned char>(spelled[at - 1]))) { continue; }
            const auto end = at + name.size();
            if (end == spelled.size() or not std::isdigit(static_cast<unsigned char>(spelled[end]))) { return false; }
          }
        }
        out = text;
        return true;
      }
    };

    /// "subst" as one interned key, so a memo entry is a pair of pointers.
    auto SubstId(TypeSubst const &subst) -> TypeId {
      auto types = subst.Types;
      auto comps = subst.Comps;
      std::ranges::sort(types);
      std::ranges::sort(comps);
      auto key = InstanceKey();
      for (auto const &[param, type] : types) {
        key.Push(Tag::Param, param);
        key.PushId(type);
      }
      for (auto const &[param, value] : comps) {
        key.Push(Tag::Comp, param);
        key.Push(Tag::Comp, value);
      }
      for (const auto pack : subst.TypePacks) { key.Push(Tag::Pos, pack); }
      for (const auto pack : subst.CompPacks) { key.Push(Tag::Len, pack); }
      return InternTypeKey(std::move(key));
    }
  }

  auto ParamIdOf(
    const TypeId id)
    -> std::uint64_t {
    if (id == nullptr) { return 0; }
    auto const &head = HeadOf(id);
    return head.Kind == Tag::Param ? head.ParamId : 0;
  }

  auto CompParamIdOf(
    const TypeId id)
    -> std::uint64_t {
    if (id == nullptr or id->Words.size() != 1 or TagOf(id->Words[0]) != Tag::Comp) { return 0; }
    return CompParamIdOfText(PayloadOf(id->Words[0]));
  }

  auto CompParamText(
    const std::uint64_t param_id)
    -> std::uint64_t {
    return static_cast<std::uint64_t>(utils::Intern("C" + std::to_string(param_id)));
  }

  auto CompParamIdOfText(
    const std::uint64_t interned)
    -> std::uint64_t {
    const auto text = utils::InternedText(static_cast<utils::InternedId>(interned));
    if (text.size() < 2 or text[0] != 'C') { return 0; }
    auto param_id = std::uint64_t{0};
    const auto [end, ec] = std::from_chars(text.data() + 1, text.data() + text.size(), param_id);
    return ec == std::errc() and end == text.data() + text.size() ? param_id : 0;
  }

  auto CompPackElements(
    const StrView identity)
    -> std::optional<std::vector<StrView>> {
    if (not identity.starts_with("P(") or not identity.ends_with(')')) { return std::nullopt; }
    const auto inner = identity.substr(2, identity.size() - 3);
    auto out = std::vector<StrView>();
    if (inner.empty()) { return out; }
    auto depth = 0;
    auto start = 0uz;
    for (auto i = 0uz; i < inner.size(); ++i) {
      if (inner[i] == '(') { ++depth; }
      else if (inner[i] == ')') { --depth; }
      else if (depth == 0 and inner[i] == ',' and i + 1 < inner.size() and inner[i + 1] == ' ') {
        out.push_back(inner.substr(start, i - start));
        start = i + 2;
        ++i;
      }
    }
    out.push_back(inner.substr(start));
    return out;
  }

  auto SubstituteTypeId(
    const TypeId id,
    TypeSubst const &subst)
    -> TypeId {
    if (id == nullptr) { return nullptr; }
    if (subst.IsEmpty()) { return id->HasUnresolved ? nullptr : id; }
    auto rewriter = Rewriter(subst, SubstId(subst));
    const auto result = rewriter.Part(id);
    return result == nullptr or result->HasUnresolved ? nullptr : result;
  }

  auto HeadOf(
    const TypeId id)
    -> TypeIdHead const& {
    // Keys and their parts are interned for the whole process, so the answer is kept for as long, and on the key.
    static auto memo = std::unordered_map<TypeId, TypeIdHead>();
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
    case Tag::Param:
    case Tag::Bound:
      head.ParamId = PayloadOf(words[0]);
      break;
    case Tag::Sym:
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
          head.Members.push_back(ReadPart(rest, taken));
          rest = rest.subspan(taken);
        }
      }
      break;
    default:
      break;
    }
    return remember(std::move(head));
  }

  auto BareTypeId(
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

  auto ParamsOf(
    const TypeId id)
    -> TypeIdParams const& {
    // Keys and their parts are interned for the whole process, so the answer is kept for as long, and on the key.
    static auto memo = std::unordered_map<TypeId, TypeIdParams>();
    if (id != nullptr and id->CachedParams != nullptr) { return *id->CachedParams; }
    if (const auto hit = memo.find(id); hit != memo.end()) { return hit->second; }
    auto out = TypeIdParams();
    const auto add = [](std::vector<std::uint64_t> &into, const std::uint64_t x) {
      if (std::ranges::find(into, x) == into.end()) { into.push_back(x); }
    };
    const auto words = WordsOf(id);
    for (auto i = 0uz; i < words.size(); ++i) {
      switch (TagOf(words[i])) {
      case Tag::Param:
        add(out.Types, PayloadOf(words[i]));
        break;
      case Tag::Bound:
        add(out.Types, PayloadOf(words[i]));
        ++i;
        break;
      case Tag::Sym:
        ++i;
        break;
      case Tag::Id: {
        auto const &inner = ParamsOf(AsId(words[i + 1]));
        for (const auto t : inner.Types) { add(out.Types, t); }
        for (const auto c : inner.Comps) { add(out.Comps, c); }
        ++i;
        break;
      }
      case Tag::Comp: {
        // A comp identity names its parameters as "C<ParamId>" ("comp_generics::CompExprIdentity").
        const auto text = utils::InternedText(static_cast<utils::InternedId>(PayloadOf(words[i])));
        for (auto at = text.find('C'); at != StrView::npos; at = text.find('C', at + 1)) {
          auto end = at + 1;
          while (end < text.size() and std::isdigit(static_cast<unsigned char>(text[end]))) { ++end; }
          if (end > at + 1 and (at == 0 or not std::isalnum(static_cast<unsigned char>(text[at - 1])))) {
            add(out.Comps, static_cast<std::uint64_t>(utils::Intern(text.substr(at, end - at))));
          }
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

  auto ArgsOf(
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
      if (TagOf(words[0]) == Tag::Comp) {
        arg.Comp = PayloadOf(words[0]);
        words = words.subspan(1);
      }
      else {
        auto taken = std::size_t{0};
        arg.Type = ReadPart(words, taken);
        words = words.subspan(taken);
      }
      out.push_back(arg);
    }
    return out;
  }
}
