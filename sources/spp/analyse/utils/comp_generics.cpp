module spp.analyse.utils.comp_generics;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_size;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.utils.interner;
import genex;
import std;

auto spp::analyse::utils::comp_generics::FindCompMemberId(
  const TypeId owner, const StrView name, Scope const *const declared_in) -> CompId {
  // Convert the type-interned owner into a comp key, and intern
  // the comp key into the cache.
  using Tag = TypeKey::Tag;
  const auto member = InternCompKey(CompKey::OfMember(TypeIdWord(owner), name));

  // Constants defined through each other ("cmp a = Self::b",
  // "cmp b = Self::a") stay members: a constant asked for
  // again while it is being read is its member identity.
  thread_local auto reading = std::vector<Pair<TypeId, Str>>();
  if (genex::any_of(reading, [&](auto const &r) { return r.first == owner and r.second == name; })) { return member; }
  reading.emplace_back(owner, Str(name));
  struct Leave {
    ~Leave() { reading.pop_back(); }
  } const _leave;

  // The owner: a class, or a made instance of a template.
  auto const &head = scopes::HeadOf(owner);
  auto const *const tmpl = head.Kind == Tag::Symbol or head.Kind == Tag::Inst
    ? head.Symbol()
    : nullptr;
  auto const *const sym = head.Kind == Tag::Inst and tmpl != nullptr and tmpl->LinkedScope != nullptr
    ? tmpl->LinkedScope->FindTypeSymbolById(owner)
    : tmpl;
  if (sym == nullptr or sym->LinkedScope == nullptr) { return nullptr; }

  // The constant, where the owner's blocks declare it; while those are being attached, where its template's do (the
  // same constant: a value naming the instance's parameters stays a member below); before then, where the block a
  // "Self::n" is written in declares it ("declared_in", the block "Self" names).
  const auto id = MakeShared<IdentifierAst>(0uz, Str(name));
  auto const *var = static_cast<VariableSymbol const*>(nullptr);
  auto const *decl = static_cast<Scope const*>(nullptr);
  for (auto const &found : member_lookup::ScopesDeclaringVar(*sym->LinkedScope, *id)) {
    var = found.Symbol;
    decl = found.Where;
    break;
  }
  if (var == nullptr) {
    auto *const base = TypeRef::ForKindCheck(*sym, *sym->LinkedScope).Template();
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
  if (var == nullptr and declared_in != nullptr) {
    if (auto const *const found = declared_in->FindVarSymbol(id.get(), true);
      found != nullptr and found->CompTimeValue != nullptr) {
      var = found;
      decl = declared_in;
    }
  }
  if (var == nullptr or decl == nullptr) { return nullptr; }

  // Its value's identity, where it is declared, when that is a value all the way down ("scopes::IsValueCompId"); one
  // still naming a parameter or a constant, holding anything opaque, or not folding, keeps the member as its identity.
  auto const *const value = var->CompTimeValue != nullptr ? var->CompTimeValue->To<ExpressionAst>() : nullptr;
  if (value == nullptr) { return member; }
  const auto value_id = decl->CompIdOf(*value);
  return scopes::IsValueCompId(value_id) ? value_id : member;
}

/// [CHECKED]
auto spp::analyse::utils::comp_generics::NeedsSupScopesToType(
  ExpressionAst const &value) -> bool {
  // Asked of the value itself, or of each element of a pack's
  // tuple: anything but a bare literal or name answers at once,
  // so an operand is never reached.
  return type_predicates::AnyCompPart(value, nullptr, nullptr, [](ExpressionAst const &part) {
    return part.To<LiteralAst>() == nullptr and part.To<IdentifierAst>() == nullptr;
  });
}

/// [CHECKED]
auto spp::analyse::utils::comp_generics::IsCompOperator(
  ExpressionAst const &value) -> bool {
  // A comp expression is an operator if any part is a binary
  // operation ast of a parenthesized expression.
  return type_predicates::AnyCompPart(value, nullptr, nullptr, [](ExpressionAst const &part) {
    return part.To<BinaryExpressionAst>() != nullptr or part.To<ParenthesisedExpressionAst>() != nullptr;
  });
}
