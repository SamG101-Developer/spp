module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_parameter_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_resolution;
import spp.asts.convention_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_uninitialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::FunctionParameterAst::FunctionParameterAst(
  decltype(Var) &&var,
  decltype(TokColon) &&tok_colon,
  decltype(Type) type,
  const utils::OrderableTag order_tag) :
  OrderableAst(order_tag),
  Var(std::move(var)),
  TokColon(std::move(tok_colon)),
  Type(std::move(type)) {
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokColon, SppTokenType::TK_COLON, ":", this->Var ? this->Var->PosEnd() : 0);
  if (this->Var == nullptr) {
    const auto uid = spp::utils::Uid();
    const auto pos = this->Type ? this->Type->PosStart() : 0uz;
    auto var_name = MakeShared<IdentifierAst>(pos, uid);
    this->Var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);
  }
  Source.OriginalType = AstClone(Type);
}

spp::asts::FunctionParameterAst::~FunctionParameterAst() = default;

auto spp::asts::FunctionParameterAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Analyse the type. "Self" is kept, and substituted per
  // call.
  IMPORT_UTILS;
  using type_resolution::SelfPolicy;
  Type = type_resolution::AnalyseWrittenType(*Type, *sm, *meta, SelfPolicy::kKeep);

  // A template's variadic parameter holds, as a whole, an open tuple of its pack, of a length only an instance knows:
  // "Tup[Ts]" for "..b: Ts". A pack of one element type ("..b: T", "..b: S32") has no type pack of its own to name its
  // length, so it is given one: the hidden parameter an instance binds to the pack's tuple ("packs::
  // FnVariadicParamName"), declared here as an unbound variadic generic, so "b" is "Tup[VariadicPackOfb]". Then "(..b)"
  // and a forward of it ("inner(..b)") are typed alike for both (pack-design.md phase 5). Each element is a "T", so
  // the hidden parameter is held to what a "T" meets: a generic element's constraints, or a concrete element itself.
  // An element ("b.0") is typed as the pack declares it ("PackInfo"), never off the tuple. In an instance the pack is
  // its elements' parameters ("monomorphization::ExpandClosedPack").
  auto var_type = Type;
  if (auto *const variadic = To<FunctionParameterVariadicAst>(); variadic != nullptr) {
    auto element = Type;
    variadic->OverTypePack = packs::DoesTypeNameAnUnboundPack(*Type, *sm->CurrentScope);
    if (not variadic->OverTypePack) {
      const auto hidden = packs::DeclareHiddenPack(
        packs::FnVariadicParamName(*ExtractName()), *Type, variadic, Type->PosStart(), *sm->CurrentScope);
      element = hidden->WithConvention(AstClone(Type->GetConvention()));
    }
    var_type = generate::common_types::TupleType(Type->PosStart(), {element});
    var_type->Stage7_AnalyseSemantics(sm, meta);
  }

  // Create the variable for the parameter (use temp copies
  // and put them back).
  const auto ast = MakeUnique<LetStatementUninitializedAst>(
    nullptr, std::move(Var), nullptr, var_type);
  ast->Stage7_AnalyseSemantics(sm, meta);
  Var = std::move(ast->Var);

  // A parameter is declared by the signature, not by a "let"
  // a walk reaches in the body, so it is never hidden on
  // entering the scope.
  for (auto const &name : ExtractNames()) {
    sm->CurrentScope->UnrecordLetBinding(*name);
  }

  // Mark the symbol as initialized. A variadic parameter's
  // symbol holds a pack, as a variadic comp parameter's does.
  const auto conv = Type->GetConvention();
  for (auto const &name : ExtractNames()) {
    const auto sym = sm->CurrentScope->FindVarSymbol(name.get());
    memory_state::StateOf(*sm->CurrentScope, *sym).InitializedBy(*this);
    sym->MemInfo->AstBorrowed = conv;

    // A pack is no value ("PackInfo"): what it declares each element to be. Open, as a signature is analysed in its
    // template; an instance closes its copy ("monomorphization::FindOrMakeFnInstance").
    if (To<FunctionParameterVariadicAst>() != nullptr) { sym->Pack = analyse::scopes::PackInfo{.Element = Type}; }
  }
}

auto spp::asts::FunctionParameterAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *) -> void {
  IMPORT_UTILS;
  // Check the memory of each name.
  for (auto const &name : ExtractNames()) {
    const auto sym = sm->CurrentScope->FindVarSymbol(name.get());
    memory_state::StateOf(*sm->CurrentScope, *sym).InitializedBy(*this);
  }
}

auto spp::asts::FunctionParameterAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value* {
  // Generate the local variable so that the symbol table
  // receives the alloca.
  const auto _meta_guard = MetaGuard(meta);
  meta->LetStatementExplicitType = Type;
  meta->LetStatementFromUninitialized = true;

  // It's not uninitialized but as the value is external
  // we need this behaviour
  Var->Stage11_CodeGen(sm, meta, ctx);
  return nullptr;
}

auto spp::asts::FunctionParameterAst::ExtractNames() const -> Vec<Shared<IdentifierAst>> {
  // Forward to the variable declaration.
  return Var->ExtractNames();
}

auto spp::asts::FunctionParameterAst::ExtractName() const -> Shared<IdentifierAst> {
  // Forward to the variable declaration.
  return Var->ExtractName();
}

SPP_MOD_END
