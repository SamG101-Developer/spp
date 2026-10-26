module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.comp_generics;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_tree;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
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
  thread_local auto reading = Vec<Pair<TypeId, Str>>();
  if (genex::any_of(reading, [&](auto const &r) { return r.first == owner and r.second == name; })) { return member; }
  reading.emplace_back(owner, Str(name));
  struct Leave {
    ~Leave() { reading.pop_back(); }
  } const _leave_guard;

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
    auto const &normal_blocks = sym->LinkedScope->Tree()->NormalSupBlocks;
    if (const auto blocks = normal_blocks.find(base); blocks != normal_blocks.end()) {
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

auto spp::analyse::utils::comp_generics::ExpectCompParamTypes(
  GenericArgumentGroupAst const &args, GenericParameterGroupAst const &params, Scope const &params_scope) -> void {
  // A parameter typed by another one ("cmp n: T") has no type to
  // give until that one is bound, and a pack's is its elements'.
  const auto all_args = args.Args | genex::views::ptr | genex::to<Vec>();
  const auto bound = type_resolution::ParamsBoundByArgs(all_args, params.Params | genex::views::ptr | genex::to<Vec>());
  for (auto i = 0uz; i < all_args.size(); ++i) {
    auto const *const param = bound[i];
    if (not all_args[i]->IsCompArg() or param == nullptr or param->IsVariadic() or param->CompType == nullptr
      or not type_predicates::IsTypeConcrete(*param->CompType, params_scope)) { continue; }
    all_args[i]->ExpectedCompType = MakeShared<TypeRef>(TypeRef::Of(*param->CompType, params_scope));
  }
}

auto spp::analyse::utils::comp_generics::FoldCompCall(
  ExpressionAst &value, ScopeManager *sm, asts::meta::CompilerMetaData *meta) -> Unique<ExpressionAst> {
  IMPORT_UTILS;
  const auto has_call = type_predicates::AnyCompPart(value, nullptr, nullptr, [](ExpressionAst const &part) {
    const auto postfix = part.To<PostfixExpressionAst>();
    return postfix != nullptr and postfix->Op->To<PostfixExpressionOperatorFunctionCallAst>() != nullptr;
  });
  const auto unfolded_op = [&] {
    if (not IsCompOperator(value)) { return false; }
    const auto id = sm->CurrentScope->CompIdOf(value);
    return scopes::IsClosedCompId(id) and not scopes::IsValueCompId(id);
  };
  if ((not has_call and not unfolded_op()) or sm->CurrentScope->IsInTemplate()) { return nullptr; }

  // Folded with its own walk over the scope it is written in, as a "cmp" statement's value and an array literal's
  // repeat count are; what the walk sets is put back, but the result, which is carried out.
  auto tm = sm->NewManagerAt(sm->CurrentScope);
  {
    const auto _meta_guard = asts::meta::MetaGuard(meta);
    meta->CompTimeResult = nullptr;
    value.EvaluateCompTime(&tm, meta);
  }
  RaiseIf<SppCompTimeConstantError>(meta->CompTimeResult == nullptr, ERR_ARGS(value));
  return std::move(meta->CompTimeResult);
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

namespace {
  /// The constants "CompTimeResolutionGuard" holds, innermost last.
  thread_local spp::Vec<VariableSymbol const*> resolving_constants;
}

SPP_MOD_BEGIN
CompTimeResolutionGuard::CompTimeResolutionGuard(
  VariableSymbol const *const sym) {
  resolving_constants.emplace_back(sym);
}

CompTimeResolutionGuard::~CompTimeResolutionGuard() {
  resolving_constants.pop_back();
}

auto CompTimeResolutionGuard::IsResolving(
  VariableSymbol const *const sym) -> bool {
  return genex::contains(resolving_constants, sym);
}

SPP_MOD_END
