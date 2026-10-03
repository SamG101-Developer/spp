module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.comp_key;
import spp.utils.numbers;
import spp.utils.types;
import genex;
import std;
import numex.big_int;

namespace spp::analyse::scopes {
  namespace {
    using Kind = CompNode::Part;

    /// Whether a character can be part of a value's literal ("-3_s32", "true").
    auto IsLiteralChar(const char c) -> bool {
      return std::isalnum(static_cast<unsigned char>(c)) or c == '_' or c == '-';
    }

    /// One term of the grammar at "at", advancing "at" past it.
    auto ParseTerm(StrView text, std::size_t &at) -> std::optional<CompNode> {
      if (at >= text.size()) { return std::nullopt; }
      auto node = CompNode();
      switch (text[at]) {
      case 'V': {
        const auto start = ++at;
        while (at < text.size() and IsLiteralChar(text[at])) { ++at; }
        if (at == start) { return std::nullopt; }
        node.Kind = Kind::Value;
        node.Text = Str(text.substr(start, at - start));
        return node;
      }
      case 'C': {
        const auto start = ++at;
        while (at < text.size() and std::isdigit(static_cast<unsigned char>(text[at]))) { ++at; }
        if (at == start) { return std::nullopt; }
        node.Kind = Kind::Param;
        std::from_chars(text.data() + start, text.data() + at, node.ParamId);
        return node;
      }
      case 'O': {
        const auto start = ++at;
        while (at < text.size() and std::isdigit(static_cast<unsigned char>(text[at]))) { ++at; }
        if (at == start or at >= text.size() or text[at] != ':') { return std::nullopt; }
        auto len = std::size_t{0};
        std::from_chars(text.data() + start, text.data() + at, len);
        ++at;
        if (at + len > text.size()) { return std::nullopt; }
        node.Kind = Kind::Opaque;
        node.Text = Str(text.substr(at, len));
        at += len;
        return node;
      }
      case 'M': {
        const auto start = ++at;
        while (at < text.size() and std::isdigit(static_cast<unsigned char>(text[at]))) { ++at; }
        if (at == start or at >= text.size() or text[at] != '.') { return std::nullopt; }
        std::from_chars(text.data() + start, text.data() + at, node.Type);
        const auto name_start = ++at;
        while (at < text.size() and (std::isalnum(static_cast<unsigned char>(text[at])) or text[at] == '_')) { ++at; }
        if (at == name_start) { return std::nullopt; }
        node.Kind = Kind::Member;
        node.Text = Str(text.substr(name_start, at - name_start));
        return node;
      }
      case 'P': {
        if (at + 1 >= text.size() or text[at + 1] != '(') { return std::nullopt; }
        at += 2;
        node.Kind = Kind::Pack;
        if (at < text.size() and text[at] == ')') {
          ++at;
          return node;
        }
        while (true) {
          auto elem = ParseTerm(text, at);
          if (not elem.has_value()) { return std::nullopt; }
          node.Kids.push_back(std::move(*elem));
          if (at < text.size() and text[at] == ')') {
            ++at;
            return node;
          }
          if (at + 1 >= text.size() or text[at] != ',' or text[at + 1] != ' ') { return std::nullopt; }
          at += 2;
        }
      }
      case '(': {
        ++at;
        auto lhs = ParseTerm(text, at);
        if (not lhs.has_value() or at >= text.size() or text[at] != ' ') { return std::nullopt; }
        const auto op_start = ++at;
        while (at < text.size() and text[at] != ' ') { ++at; }
        if (at == op_start or at >= text.size()) { return std::nullopt; }
        node.Text = Str(text.substr(op_start, at - op_start));
        ++at;
        auto rhs = ParseTerm(text, at);
        if (not rhs.has_value() or at >= text.size() or text[at] != ')') { return std::nullopt; }
        ++at;
        node.Kind = Kind::Op;
        node.Kids.push_back(std::move(*lhs));
        node.Kids.push_back(std::move(*rhs));
        return node;
      }
      default:
        return std::nullopt;
      }
    }

    /// An integer value as a "Value" spells it: as "IntegerLiteralAst::FromBigVal(..)->ToString()" does.
    auto IntText(numex::BigInt const &value, Str const &type) -> Str {
      return value.ToString() + "_" + type;
    }

    auto BoolText(const bool value) -> Str {
      return value ? "true" : "false";
    }

    /// A shift count, as the shift intrinsics read one ("CppVal<std::uint32_t>").
    auto ShiftCount(numex::BigInt const &count) -> std::optional<std::uint32_t> {
      if (count.IsNegative()) { return std::nullopt; }
      return static_cast<std::uint32_t>(std::stoull(count.ToString()));
    }

    /// "value" within the range of "type", as the same bits read back ("IntegerLiteralAst::FromWrappedBigVal").
    auto WrapToType(numex::BigInt const &value, Str const &type) -> std::optional<numex::BigInt> {
      const auto bounds = spp::utils::numbers::IntegerBounds().find(type);
      if (bounds == spp::utils::numbers::IntegerBounds().end()) { return std::nullopt; }
      auto const &[lower, upper] = bounds->second;
      const auto modulus = upper - lower + numex::BigInt(1);
      auto wrapped = value % modulus;
      if (wrapped.IsNegative()) { wrapped = wrapped + modulus; }
      if (wrapped > upper) { wrapped = wrapped - modulus; }
      return wrapped;
    }

    /// The identity of a comp parameter as a value ("C12"), by its "ParamId".
    auto ParamCompKey(
      const std::uint64_t param_id)
      -> Str {
      return "C" + std::to_string(param_id);
    }

    /// The identity of a constant named through a type ("Box[S32]::N"), the type by its "TypeId"'s word.
    auto MemberCompKey(
      const std::uint64_t type,
      const StrView name)
      -> Str {
      return "M" + std::to_string(type) + "." + Str(name);
    }

    /// The identity of a value that is not one of the grammar's own: its spelling, length-prefixed.
    auto OpaqueCompKey(
      const StrView spelling)
      -> Str {
      return "O" + std::to_string(spelling.size()) + ":" + Str(spelling);
    }

    auto Rewrite(
      CompNode const &node,
      std::function<std::optional<CompNode>(std::uint64_t)> const &bound,
      std::function<bool(std::uint64_t)> const &is_pack,
      std::function<std::optional<CompNode>(std::uint64_t, StrView)> const &member) -> std::optional<CompNode> {
      switch (node.Kind) {
      case Kind::Value:
      case Kind::Opaque:
        return node;
      case Kind::Member:
        return member != nullptr ? member(node.Type, node.Text) : std::optional(node);
      case Kind::Param: {
        auto value = bound != nullptr ? bound(node.ParamId) : std::nullopt;
        return value.has_value() ? std::move(value) : std::optional(node);
      }
      case Kind::Pack: {
        auto out = CompNode{.Kind = Kind::Pack, .Text = {}, .ParamId = 0, .Kids = {}, .Type = 0};
        for (auto const &elem : node.Kids) {
          auto rewritten = Rewrite(elem, bound, is_pack, member);
          if (not rewritten.has_value()) { return std::nullopt; }
          // A pack parameter standing as one element is spread into the elements it is bound to.
          if (elem.Kind == Kind::Param and is_pack != nullptr and is_pack(elem.ParamId)
            and rewritten->Kind == Kind::Pack) {
            for (auto &part : rewritten->Kids) { out.Kids.push_back(std::move(part)); }
            continue;
          }
          out.Kids.push_back(std::move(*rewritten));
        }
        return out;
      }
      case Kind::Op: {
        auto lhs = Rewrite(node.Kids[0], bound, is_pack, member);
        auto rhs = Rewrite(node.Kids[1], bound, is_pack, member);
        if (not lhs.has_value() or not rhs.has_value()) { return std::nullopt; }
        if (lhs->Kind == Kind::Value and rhs->Kind == Kind::Value) {
          if (auto folded = FoldCompValues(node.Text, lhs->Text, rhs->Text); folded.has_value()) {
            return CompNode{.Kind = Kind::Value, .Text = std::move(*folded), .ParamId = 0, .Kids = {}, .Type = 0};
          }
        }
        auto out = CompNode{.Kind = Kind::Op, .Text = node.Text, .ParamId = 0, .Kids = {}, .Type = 0};
        out.Kids.push_back(std::move(*lhs));
        out.Kids.push_back(std::move(*rhs));
        return out;
      }
      default:
        return std::nullopt;
      }
    }
  }

  auto ParseCompKey(
    const StrView text)
    -> std::optional<CompNode> {
    auto at = std::size_t{0};
    auto node = ParseTerm(text, at);
    return node.has_value() and at == text.size() ? node : std::nullopt;
  }

  auto PrintCompKey(
    CompNode const &node)
    -> Str {
    switch (node.Kind) {
    case Kind::Value:
      return "V" + node.Text;
    case Kind::Param:
      return ParamCompKey(node.ParamId);
    case Kind::Opaque:
      return OpaqueCompKey(node.Text);
    case Kind::Member:
      return MemberCompKey(node.Type, node.Text);
    case Kind::Pack: {
      auto out = Str("P(");
      for (auto i = 0uz; i < node.Kids.size(); ++i) {
        if (i != 0) { out += ", "; }
        out += PrintCompKey(node.Kids[i]);
      }
      return out + ")";
    }
    case Kind::Op:
      return "(" + PrintCompKey(node.Kids[0]) + " " + node.Text + " " + PrintCompKey(node.Kids[1]) + ")";
    default:
      return {};
    }
  }

  auto FoldCompValues(
    const StrView op,
    const StrView lhs,
    const StrView rhs)
    -> std::optional<Str> {
    // Two bools compare for equality, and nothing else.
    const auto lhs_bool = ParseCompBool(lhs);
    const auto rhs_bool = ParseCompBool(rhs);
    if (lhs_bool.has_value() or rhs_bool.has_value()) {
      if (not lhs_bool.has_value() or not rhs_bool.has_value()) { return std::nullopt; }
      if (op == "==") { return BoolText(*lhs_bool == *rhs_bool); }
      if (op == "!=") { return BoolText(*lhs_bool != *rhs_bool); }
      return std::nullopt;
    }

    // An unsuffixed integer takes the other side's type, as it does in an expression ("n + 1" with "n: USize"); two
    // different types do not mix.
    // A shift's count is its own type ("U32"), so a shift is typed by the value shifted alone.
    const auto l = ParseCompInt(lhs);
    const auto r = ParseCompInt(rhs);
    if (not l.has_value() or not r.has_value()) { return std::nullopt; }
    auto const &[a, l_type] = *l;
    auto const &[b, r_type] = *r;
    const auto is_shift = op == "<<" or op == ">>";
    if (not is_shift and not l_type.empty() and not r_type.empty() and l_type != r_type) { return std::nullopt; }
    const auto type = is_shift or not l_type.empty() ? l_type : r_type;
    const auto by_zero = b == numex::BigInt(0);

    if (op == "+") { return IntText(a + b, type); }
    if (op == "-") { return IntText(a - b, type); }
    if (op == "*") { return IntText(a * b, type); }
    if (op == "/") { return by_zero ? std::nullopt : std::optional(IntText(a / b, type)); }
    if (op == "%") { return by_zero ? std::nullopt : std::optional(IntText(a % b, type)); }
    if (op == "|") { return IntText(a | b, type); }
    if (op == "&") { return IntText(a & b, type); }
    if (op == "^") { return IntText(a ^ b, type); }
    if (op == "<<") {
      const auto count = ShiftCount(b);
      const auto wrapped = count.has_value() ? WrapToType(a << *count, type) : std::nullopt;
      return wrapped.has_value() ? std::optional(IntText(*wrapped, type)) : std::nullopt;
    }
    if (op == ">>") {
      const auto count = ShiftCount(b);
      return count.has_value() ? std::optional(IntText(a >> *count, type)) : std::nullopt;
    }
    if (op == "==") { return BoolText(a == b); }
    if (op == "!=") { return BoolText(a != b); }
    if (op == "<") { return BoolText(a < b); }
    if (op == "<=") { return BoolText(a <= b); }
    if (op == ">") { return BoolText(a > b); }
    if (op == ">=") { return BoolText(a >= b); }
    return std::nullopt;
  }

  auto RewriteCompKey(
    CompNode const &node,
    std::function<std::optional<CompNode>(std::uint64_t)> const &bound,
    std::function<bool(std::uint64_t)> const &is_pack,
    std::function<std::optional<CompNode>(std::uint64_t, StrView)> const &member)
    -> std::optional<CompNode> {
    return Rewrite(node, bound, is_pack, member);
  }

  auto ParseCompInt(
    const StrView literal)
    -> std::optional<Pair<numex::BigInt, Str>> {
    const auto underscore = literal.rfind('_');
    if (underscore == StrView::npos or underscore == 0) { return std::nullopt; }
    const auto digits = literal.substr(0, underscore);
    const auto magnitude = digits.front() == '-' ? digits.substr(1) : digits;
    if (magnitude.empty() or not genex::all_of(magnitude, [](const char c) {
      return std::isdigit(static_cast<unsigned char>(c)) != 0;
    })) { return std::nullopt; }
    return Pair<numex::BigInt, Str>{numex::BigInt(Str(digits)), Str(literal.substr(underscore + 1))};
  }

  auto ParseCompBool(
    const StrView literal)
    -> std::optional<bool> {
    if (literal == "true") { return true; }
    if (literal == "false") { return false; }
    return std::nullopt;
  }

  auto CompKeyParams(
    CompNode const &node)
    -> std::vector<std::uint64_t> {
    auto out = std::vector<std::uint64_t>();
    node.Visit([&out](CompNode const &part) { if (part.Kind == Kind::Param) { out.push_back(part.ParamId); } });
    return out;
  }

  auto CompKeyMembers(
    CompNode const &node)
    -> std::vector<std::pair<std::uint64_t, Str>> {
    auto out = std::vector<std::pair<std::uint64_t, Str>>();
    node.Visit([&out](CompNode const &part) {
      if (part.Kind == Kind::Member) { out.emplace_back(part.Type, part.Text); }
    });
    return out;
  }
}
