module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_argument_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.order_utils;
import spp.analyse.utils.type_compare;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ptr;
import genex;

namespace spp::asts {
  namespace {
    /// The names given more than once among a group's keyword
    /// arguments of one kind. Flag for if we are looking at type
    /// or comp args.
    auto DuplicateNames(
      Vec<GenericArgumentAst*> const &keyword_args,
      const bool comp) {
      // Track all the generic arguments' names into a single
      // vector.
      auto names = Vec<TypeAst*>();
      for (const auto x : keyword_args) {
        if ((x->CompVal != nullptr) == comp) { names.EmplaceBack(x->Name.get()); }
      }

      // Grab the first set of duplicate into a new vector
      // that we can report as errors.
      return names
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<Vec>();
    }
  }
}

SPP_MOD_BEGIN
auto GenericArgumentGroupAst::NewEmpty() -> Unique<GenericArgumentGroupAst> {
  // Empty ast.
  return MakeUnique<GenericArgumentGroupAst>(
    nullptr, decltype(Args)(), nullptr);
}

auto GenericArgumentGroupAst::FromParams(
  GenericParameterGroupAst const &generic_params) -> Unique<GenericArgumentGroupAst> {
  // Create the list of arguments, initially empty.
  auto mapped_args = Vec<Unique<GenericArgumentAst>>();

  for (auto const &param : generic_params.Params) {
    // Map type generic parameters to keyword type arguments.
    if (param->CompType == nullptr) {
      auto val = AstClone(param->Name);
      auto arg = GenericArgumentAst::NewType(param->Name, std::move(val));
      mapped_args.EmplaceBack(std::move(arg));
    }

    // Map comptime generic parameters to keyword comptime arguments.
    else {
      auto val = IdentifierAst::FromType(*param->Name);
      auto arg = GenericArgumentAst::NewComp(param->Name, std::move(val));
      mapped_args.EmplaceBack(std::move(arg));
    }
  }

  // Place the arguments into a group AST.
  auto arg_group = NewEmpty();
  arg_group->Args = std::move(mapped_args);
  return arg_group;
}

auto GenericArgumentGroupAst::FromMap(
  analyse::utils::type_compare::GenericInferenceMap const &map) -> Unique<GenericArgumentGroupAst> {
  // Create the list of arguments, initially empty.
  auto mapped_args = Vec<Unique<GenericArgumentAst>>();

  for (auto const &[arg_name, arg_val] : std::move(map)) {
    // Map type ASTs to keyword type arguments.
    if (const auto arg_val_for_type = arg_val->To<TypeAst>()) {
      auto val = AstCloneShared(arg_val_for_type);
      auto arg = GenericArgumentAst::NewType(arg_name, std::move(val));
      mapped_args.EmplaceBack(std::move(arg));
    }

    // Map expression ASTs to keyword comptime arguments.
    else if (auto *arg_val_for_comp = arg_val->To<ExpressionAst>()) {
      auto val = AstClone(arg_val_for_comp);
      auto arg = GenericArgumentAst::NewComp(arg_name, std::move(val));
      mapped_args.EmplaceBack(std::move(arg));
    }
  }

  // Place the arguments into a group AST.
  return MakeUnique<GenericArgumentGroupAst>(
    nullptr, std::move(mapped_args), nullptr);
}

GenericArgumentGroupAst::GenericArgumentGroupAst(
  decltype(TokL) &&tok_l,
  decltype(Args) &&args,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Args(std::move(args)),
  TokR(std::move(tok_r)) {
}

GenericArgumentGroupAst::~GenericArgumentGroupAst() = default;

auto GenericArgumentGroupAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL != nullptr ? TokL->PosStart() : Args.IsEmpty() ? 0 : Args.Front()->PosStart();
}

auto GenericArgumentGroupAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR != nullptr ? TokR->PosEnd() : Args.IsEmpty() ? 0 : Args.Back()->PosEnd();
}

auto GenericArgumentGroupAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<GenericArgumentGroupAst>(
    AstClone(TokL),
    AstCloneVec(Args),
    AstClone(TokR));
}

auto GenericArgumentGroupAst::ToString() const -> Str {
  SPP_STRING_START;
  if (not Args.IsEmpty()) {
    // Separators go between the arguments only, so there is no
    // trailing ", " before the "]".
    SPP_STRING_APPEND_RAW("[");
    for (auto i = 0uz; i < Args.Len(); ++i) {
      if (i != 0) { SPP_STRING_APPEND_RAW(", "); }
      SPP_STRING_APPEND(Args[i]);
    }
    SPP_STRING_APPEND_RAW("]");
  }
  SPP_STRING_END;
}

auto GenericArgumentGroupAst::operator==(
  GenericArgumentGroupAst const &other) const -> bool {
  if (Args.Len() != other.Args.Len()) {
    return false;
  }

  for (std::size_t i = 0; i < Args.Len(); i++) {
    if (*Args[i] != *other.Args[i]) { return false; }
  }

  return true;
}

auto GenericArgumentGroupAst::operator+=(
  const GenericArgumentGroupAst &other) -> GenericArgumentGroupAst& {
  MergeGenerics(AstCloneVec(other.Args));
  return *this;
}

auto GenericArgumentGroupAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  using analyse::errors::SppIdentifierDuplicateError;
  using analyse::errors::SppOrderInvalidError;

  // Check there are no duplicate type or comp argument names.
  const auto keyword_args = GetKeywordArgs();
  const auto type_arg_names = DuplicateNames(keyword_args, false);
  RaiseIf<SppIdentifierDuplicateError>(
    not type_arg_names.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(*type_arg_names[0], *type_arg_names[1], "keyword generic type argument"));

  const auto comp_arg_names = DuplicateNames(keyword_args, true);
  RaiseIf<SppIdentifierDuplicateError>(
    not comp_arg_names.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(*comp_arg_names[0], *comp_arg_names[1], "keyword generic comp argument"));

  // Check the arguments are in the correct order.
  const auto unordered_args = analyse::utils::order_utils::DoOrderArgs(Args
    | genex::views::ptr
    | genex::views::cast_dynamic<mixins::OrderableAst*>()
    | genex::to<Vec>());

  RaiseIf<SppOrderInvalidError>(
    not unordered_args.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(unordered_args[0].first, *unordered_args[0].second, unordered_args[1].first, *unordered_args[1].second));

  // Analyse the arguments.
  for (auto const &x : Args) { x->Stage7_AnalyseSemantics(sm, meta); }
}

auto GenericArgumentGroupAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the arguments for memory issues.
  for (auto const &x : Args) { x->Stage8_CheckMemory(sm, meta); }
}

auto GenericArgumentGroupAst::At(
  const char *key) const -> GenericArgumentAst const* {
  // Find the keyword argument with the matching key. A type and a comp parameter cannot share a name, so neither can
  // their arguments.
  for (auto const &arg : Args) {
    if (arg->Name != nullptr and arg->Name->LastTypePart()->Name == key) { return arg.get(); }
  }
  return nullptr;
}

auto GenericArgumentGroupAst::MergeGenerics(
  decltype(Args) &&other_args) -> void {
  // Append the other arguments to this argument group, checking
  // named duplicates.
  for (auto &&other_arg : std::move(other_args)) {
    if (other_arg->Name == nullptr) {
      const auto err = "generic argument '" + other_arg->ToString() + "' is still positional at a merge";
      Raise<analyse::errors::SppInternalCompilerError>({}, ERR_ARGS(*other_arg, err));
    }
    const auto *name = other_arg->Name->ToUnchecked<TypeIdentifierAst>()->Name.c_str();
    if (At(name) != nullptr) { continue; }
    Args.EmplaceBack(std::move(other_arg));
  }
}

auto GenericArgumentGroupAst::GetTypeArgs() const -> Vec<GenericArgumentAst*> {
  // Filter by the kind of value.
  return Args
    | genex::views::filter([](auto const &arg) { return arg->TypeVal != nullptr; })
    | genex::views::transform([](auto const &arg) { return arg.get(); })
    | genex::to<Vec>();
}

auto GenericArgumentGroupAst::GetCompArgs() const -> Vec<GenericArgumentAst*> {
  // Filter by the kind of value.
  return Args
    | genex::views::filter([](auto const &arg) { return arg->CompVal != nullptr; })
    | genex::views::transform([](auto const &arg) { return arg.get(); })
    | genex::to<Vec>();
}

auto GenericArgumentGroupAst::GetKeywordArgs() const -> Vec<GenericArgumentAst*> {
  // Filter by whether the argument is named.
  return Args
    | genex::views::filter([](auto const &arg) { return arg->Name != nullptr; })
    | genex::views::transform([](auto const &arg) { return arg.get(); })
    | genex::to<Vec>();
}

auto GenericArgumentGroupAst::GetPositionalArgs() const -> Vec<GenericArgumentAst*> {
  // Filter by whether the argument is named.
  return Args
    | genex::views::filter([](auto const &arg) { return arg->Name == nullptr; })
    | genex::views::transform([](auto const &arg) { return arg.get(); })
    | genex::to<Vec>();
}

auto GenericArgumentGroupAst::GetAllArgs() const -> Vec<GenericArgumentAst*> {
  // Convert args to raw pointers.
  return Args
    | genex::views::transform([](auto const &arg) { return arg.get(); })
    | genex::to<Vec>();
}

SPP_MOD_END
