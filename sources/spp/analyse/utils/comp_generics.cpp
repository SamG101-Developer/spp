module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.comp_generics;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_size;
import spp.lex.lexer;
import spp.lex.tokens;
import spp.parse.parser_spp;
import spp.utils.interner;
import genex;
import std;
import numex.big_dec;
import numex.big_int;

auto spp::analyse::utils::comp_generics::FoldCompExpr(
  ExpressionAst const &expr,
  Scope const &scope)
  -> Unique<ExpressionAst> {
  // What its identity folds to ("CompKey"), as an ast ("Scope::CompAstOf"): a value, or a pack every element of which
  // is one ("cmp ..ns" is a tuple of values, closed when every element is, as a type pack's tuple is). Anything still
  // naming a parameter, or an opaque value, is not closed.
  using Kind = scopes::CompNode::Part;
  auto const *const node = scopes::CompNodeOf(scope.CompIdOf(expr));
  const auto closed = node != nullptr and not node->Any([](scopes::CompNode const &part) {
    return part.Kind != Kind::Value and part.Kind != Kind::Pack;
  });
  return closed ? scope.CompAstOf(*node) : nullptr;
}

auto spp::analyse::utils::comp_generics::CompValueAst(
  const StrView literal)
  -> Unique<ExpressionAst> {
  if (const auto value = scopes::ParseCompBool(literal); value.has_value()) { return BooleanLiteralAst::FromCppVal(*value); }
  if (const auto value = scopes::ParseCompInt(literal); value.has_value()) {
    return IntegerLiteralAst::FromBigVal(value->first, value->second);
  }
  return nullptr;
}

auto spp::analyse::utils::comp_generics::FindCompMemberId(
  const scopes::TypeId owner,
  const StrView name)
  -> scopes::CompId {
  using Tag = scopes::InstanceKey::Tag;
  using Kind = scopes::CompNode::Part;
  const auto member = scopes::InternCompKey(scopes::CompNode::OfMember(scopes::TypeIdWord(owner), name));

  // Constants defined through each other ("cmp a = Self::b", "cmp b = Self::a") stay members.
  thread_local auto depth = 0;
  if (depth > 16) { return member; }
  ++depth;
  struct Leave { ~Leave() { --depth; } } const _leave;

  // The owner: a class, or a made instance of a template.
  auto const &head = scopes::HeadOf(owner);
  auto const *const tmpl = head.Kind == Tag::Symbol or head.Kind == Tag::Inst
    ? head.Symbol()
    : nullptr;
  auto const *const sym = head.Kind == Tag::Inst and tmpl != nullptr and tmpl->LinkedScope != nullptr
    ? tmpl->LinkedScope->TypeSymbolOf(owner)
    : tmpl;
  if (sym == nullptr or sym->LinkedScope == nullptr) { return 0; }

  // The constant, where the owner's blocks declare it; while those are being attached, where its template's do (the
  // same constant: a value naming the instance's parameters stays a member below).
  const auto id = MakeShared<IdentifierAst>(0uz, Str(name));
  auto const *var = static_cast<VariableSymbol const*>(nullptr);
  auto const *decl = static_cast<Scope const*>(nullptr);
  for (auto const &found : member_lookup::ScopesDeclaringVar(*sym->LinkedScope, *id)) {
    var = found.Symbol;
    decl = found.Where;
    break;
  }
  if (var == nullptr) {
    auto *const base = TypeRef::OfKind(*sym, *sym->LinkedScope).Template();
    if (const auto blocks = ScopeManager::NormalSupBlocks.find(base); blocks != ScopeManager::NormalSupBlocks.end()) {
      for (auto const *block : blocks->second) {
        if (auto const *const found = block->FindVarSymbol(id.get(), true); found != nullptr) {
          var = found;
          decl = block;
          break;
        }
      }
    }
  }
  if (var == nullptr or decl == nullptr) { return 0; }

  // Its value's identity, where it is declared; one still naming a parameter, or holding anything opaque, keeps the
  // member as its identity.
  auto const *const value = var->CompTimeValue != nullptr ? var->CompTimeValue->To<ExpressionAst>() : nullptr;
  if (value == nullptr) { return member; }
  const auto value_id = decl->CompIdOf(*value);
  auto const *const node = scopes::CompNodeOf(value_id);
  const auto open = node == nullptr or node->Any([](scopes::CompNode const &part) {
    return part.Kind == Kind::Param or part.Kind == Kind::Opaque or part.Kind == Kind::Member;
  });
  return open ? member : value_id;
}

auto spp::analyse::utils::comp_generics::IsCompExpression(
  ExpressionAst const &value)
  -> bool {
  return type_predicates::AnyCompPart(value, nullptr, nullptr, [](ExpressionAst const &part) {
    return part.To<LiteralAst>() == nullptr and part.To<IdentifierAst>() == nullptr;
  });
}

auto spp::analyse::utils::comp_generics::SubstituteCompSelf(
  ExpressionAst const &value,
  TypeAst const &with)
  -> Shared<ExpressionAst> {
  const auto rewrite = [&with](ExpressionAst const &part) { return AstClone(SubstituteCompSelf(part, with)); };
  if (auto const *const type = value.To<TypeAst>(); type != nullptr) { return type->SubstituteSelf(with); }
  if (auto const *const tup = value.To<TupleLiteralAst>(); tup != nullptr) {
    auto elems = Vec<Unique<ExpressionAst>>();
    for (auto const &elem : tup->Elems) { elems.EmplaceBack(rewrite(*elem)); }
    return MakeShared<TupleLiteralAst>(AstClone(tup->TokL), std::move(elems), AstClone(tup->TokR));
  }
  if (auto const *const paren = value.To<ParenthesisedExpressionAst>(); paren != nullptr) {
    return MakeShared<ParenthesisedExpressionAst>(AstClone(paren->TokL), rewrite(*paren->Expr), AstClone(paren->TokR));
  }
  if (auto const *const bin = value.To<BinaryExpressionAst>(); bin != nullptr) {
    if (const auto [lhs, rhs] = bin->Operands(); lhs != nullptr and rhs != nullptr) {
      return MakeShared<BinaryExpressionAst>(rewrite(*lhs), AstClone(bin->TokOp), rewrite(*rhs));
    }
  }
  if (auto const *const pf = value.To<PostfixExpressionAst>(); pf != nullptr) {
    return MakeShared<PostfixExpressionAst>(rewrite(*pf->Lhs), AstClone(pf->Op));
  }
  return AstCloneShared(&value);
}

auto spp::analyse::utils::comp_generics::IsCompOperator(
  ExpressionAst const &value)
  -> bool {
  return type_predicates::AnyCompPart(value, nullptr, nullptr, [](ExpressionAst const &part) {
    return part.To<BinaryExpressionAst>() != nullptr or part.To<ParenthesisedExpressionAst>() != nullptr;
  });
}

namespace {
  /// The value each opaque comp identity ("O<len>:<spelling>") stands for, recorded as it is keyed ("RawCompNode"):
  /// no identity names it, so this is the only way back to it ("OpaqueCompValue"). Keys are interned for the process,
  /// and the values are clones owned here, their names recording what they mean where they were first keyed.
  auto OpaqueValues() -> spp::Map<spp::analyse::scopes::CompId, spp::Shared<spp::asts::ExpressionAst>>& {
    static auto values = spp::Map<spp::analyse::scopes::CompId, spp::Shared<spp::asts::ExpressionAst>>();
    return values;
  }

  /// What "Self" is where "scope" is, read there: the nearest "sup" block's type as written ("Box[T]", over the block's
  /// own parameters, which its instantiation binds), else the nearest class over its own parameters. A method's own
  /// "sup $M ext FunXxx" block (stage 1 lowers each method into one) is passed over: its "Self" is the mock, not the
  /// method's owner.
  auto SelfOwnerIn(spp::analyse::scopes::Scope const &scope) -> spp::analyse::scopes::TypeId {
    for (auto const *s = &scope; s != nullptr; s = s->Parent) {
      if (s->AstNode == nullptr) { continue; }
      if (s->AstNode->To<spp::asts::ClassPrototypeAst>() != nullptr and s->LinkedTypeSymbol != nullptr) {
        return s->TypeIdOf(*s->LinkedTypeSymbol->GnSelfName());
      }
      if (const auto name = spp::asts::AstNameOrNull(s->AstNode); name != nullptr and not name->IsCompilerGeneratedType()) {
        return s->TypeIdOf(*name);
      }
    }
    return nullptr;
  }

  /// What an expression's identity is before folding ("CompKey"): the grammar's parts as the expression is made of
  /// them ("scopes::CompNode").
  auto RawCompNode(
    spp::asts::ExpressionAst const &expr, spp::analyse::scopes::Scope const &scope) -> spp::analyse::scopes::CompNode {
    using namespace spp::asts;
    namespace scopes = spp::analyse::scopes;
    using Kind = scopes::CompNode::Part;
    const auto leaf = [](const Kind kind, spp::Str text) {
      return scopes::CompNode{.Kind = kind, .Text = std::move(text), .ParamId = 0, .Kids = {}, .Type = 0};
    };

    // A literal is its own value, spelled canonically: "0x10_uz" and "16_uz" are one value.
    if (const auto lit = expr.To<IntegerLiteralAst>(); lit != nullptr) {
      return leaf(Kind::Value, IntegerLiteralAst::FromBigVal(lit->BigVal(), lit->Type)->ToString());
    }
    if (const auto lit = expr.To<BooleanLiteralAst>(); lit != nullptr) {
      return leaf(Kind::Value, lit->CppVal() ? "true" : "false");
    }

    // A pack is its elements' identities, each on its own, so a parameter among them is named by identity (and read,
    // and substituted, as an element) however the pack is spelled or bound: "(n, 1_uz)" with "n" bound to "1_uz" is
    // "P(V1_uz, V1_uz)", as "(1_uz, 1_uz)" is.
    if (const auto tup = expr.To<TupleLiteralAst>(); tup != nullptr) {
      auto pack = leaf(Kind::Pack, {});
      for (auto const &elem : tup->Elems) { pack.Kids.push_back(RawCompNode(*elem, scope)); }
      return pack;
    }
    if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
      return RawCompNode(*paren->Expr, scope);
    }

    // A comp generic is the parameter at the end of its chain of bindings to other generics - an inherited "w" bound
    // to the block's own "w" is that "w" - however it is spelled here. A binding to a value is that value.
    if (const auto id = expr.To<IdentifierAst>(); id != nullptr) {
      auto const *var = scope.FindVarSymbol(id);
      if (var == nullptr or not var->IsGn()) { return leaf(Kind::Opaque, expr.ToString()); }
      var = var->AsBound(scope);
      if (auto const *const value = var->BoundCompVal(); value != nullptr and value->To<IdentifierAst>() == nullptr) {
        return RawCompNode(*value, scope);
      }
      return scopes::CompNode::OfParam(var->ParamId());
    }

    // An operation is its operands' identities under its operator, bracketed so precedence is explicit. The operator
    // is spelled from its token's kind: "<<" is parsed from two "<" tokens. An analysed operation is still this node
    // (its call is held beside it), so it reads as its operator.
    if (const auto bin = expr.To<BinaryExpressionAst>(); bin != nullptr) {
      if (const auto [lhs, rhs] = bin->Operands(); lhs != nullptr and rhs != nullptr) {
        auto op = leaf(Kind::Op, spp::lex::TokToString(bin->TokOp->TokenType));
        op.Kids.push_back(RawCompNode(*lhs, scope));
        op.Kids.push_back(RawCompNode(*rhs, scope));
        return op;
      }
    }

    // A constant named through a type ("Self::mo_seq_cst", "T::SIZE") is that type's identity and the constant's name,
    // so a substitution rewrites the type as it would anywhere. "Self" is the type it is where written, read there
    // ("SelfOwnerIn"): only read, it is never instantiated from, so it need not wait to be substituted. Through a closed
    // type, the constant is read now (its value, where it folds); through an open one it is read once a substitution
    // closes it ("scopes::SubstituteCompId").
    if (const auto pf = expr.To<PostfixExpressionAst>(); pf != nullptr) {
      auto const *const lhs_type = pf->Lhs->To<TypeAst>();
      auto const *const member = pf->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>();
      const auto owner = lhs_type == nullptr or member == nullptr ? nullptr
        : lhs_type->IsSelfType() ? SelfOwnerIn(scope)
        : scope.TypeIdOf(*lhs_type);
      if (owner != nullptr) {
        auto const *const read = scopes::IsClosedTypeId(owner)
          ? scopes::CompNodeOf(scopes::CompMemberIdOf(owner, member->Name->ToView()))
          : nullptr;
        if (read != nullptr) { return *read; }
        return scopes::CompNode::OfMember(scopes::TypeIdWord(owner), member->Name->ToView());
      }
    }

    // Anything else is its spelling ("x.f()"), length-prefixed so the identity still parses. As no identity names it,
    // the value is recorded under it, its names recording what they mean here ("OpaqueCompValue").
    auto opaque = leaf(Kind::Opaque, expr.ToString());
    if (const auto id = scopes::InternCompKey(opaque); not OpaqueValues().contains(id)) {
      auto recorded = spp::asts::AstCloneShared(&expr);
      spp::analyse::utils::type_resolution::RecordCompParts(*recorded, scope);
      OpaqueValues().emplace(id, std::move(recorded));
    }
    return opaque;
  }
}

auto spp::analyse::utils::comp_generics::OpaqueCompValue(
  const scopes::CompId id)
  -> ExpressionAst const* {
  const auto hit = OpaqueValues().find(id);
  return hit != OpaqueValues().end() ? hit->second.get() : nullptr;
}

auto spp::analyse::utils::comp_generics::ClearOpaqueCompValues()
  -> void {
  OpaqueValues().clear();
}

auto spp::analyse::utils::comp_generics::CompKey(
  ExpressionAst const &expr, Scope const &scope) -> scopes::CompNode {
  // A closed value is what it folds to: "1_uz + 1_uz", "n + 1_uz" with "n" bound to "1_uz", and "2_uz" are one value.
  auto raw = RawCompNode(expr, scope);
  auto folded = scopes::RewriteCompKey(raw);
  return folded.has_value() ? std::move(*folded) : std::move(raw);
}
