module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_argument_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.order_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.algorithms;
import spp.utils.ptr;
import ankerl.unordered_dense;

SPP_MOD_BEGIN
auto spp::asts::GenericArgumentGroupAst::NewEmpty()
    -> Unique<GenericArgumentGroupAst> {
    // Empty ast.
    return MakeUnique<GenericArgumentGroupAst>(
        nullptr, decltype(Args)(), nullptr);
}

auto spp::asts::GenericArgumentGroupAst::FromParams(
    GenericParameterGroupAst const &generic_params)
    -> Unique<GenericArgumentGroupAst> {
    // Create the list of arguments, initially empty.
    auto mapped_args = Vec<Unique<GenericArgumentAst>>();

    for (auto const &param : generic_params.Params) {
        // Map type generic parameters to keyword type arguments.
        if (const auto type_param = param->To<GenericParameterTypeAst>()) {
            auto val = AstClone(type_param->Name);
            auto arg = MakeUnique<GenericArgumentTypeKeywordAst>(AstClone(type_param->Name), nullptr, std::move(val));
            mapped_args.EmplaceBack(std::move(arg));
        }

        // Map comptime generic parameters to keyword comptime arguments.
        else if (const auto comp_param = param->To<GenericParameterCompAst>()) {
            auto val = AstClone(IdentifierAst::FromType(*comp_param->Name));
            auto arg = MakeUnique<GenericArgumentCompKeywordAst>(AstClone(comp_param->Name), nullptr, std::move(val));
            mapped_args.EmplaceBack(std::move(arg));
        }
    }

    // Place the arguments into a group AST.
    auto arg_group = NewEmpty();
    arg_group->Args = std::move(mapped_args);
    return arg_group;
}

auto spp::asts::GenericArgumentGroupAst::FromMap(
    analyse::utils::type_utils::GenericInferenceMap const &map)
    -> Unique<GenericArgumentGroupAst> {
    // Create the list of arguments, initially empty.
    auto mapped_args = Vec<Unique<GenericArgumentAst>>();

    for (auto [arg_name, arg_val] : std::move(map)) {
        // Map type ASTs to keyword type arguments.
        if (auto *arg_val_for_type = arg_val->To<TypeAst>()) {
            auto val = AstClone(arg_val_for_type);
            auto arg = MakeUnique<GenericArgumentTypeKeywordAst>(AstClone(arg_name), nullptr, std::move(val));
            mapped_args.EmplaceBack(std::move(arg));
        }

        // Map expression ASTs to keyword comptime arguments.
        else if (auto *arg_val_for_comp = arg_val->To<ExpressionAst>()) {
            auto val = AstClone(arg_val_for_comp);
            auto arg = MakeUnique<GenericArgumentCompKeywordAst>(AstClone(arg_name), nullptr, std::move(val));
            mapped_args.EmplaceBack(std::move(arg));
        }
    }

    // Place the arguments into a group AST.
    return MakeUnique<GenericArgumentGroupAst>(nullptr, std::move(mapped_args), nullptr);
}

auto spp::asts::GenericArgumentGroupAst::FromMap(
    analyse::utils::func_utils::InferenceFinalTypeMap const &map)
    -> Unique<GenericArgumentGroupAst> {
    // Cast the values to "ExpressionAst const*".
    auto mapped_args = analyse::utils::type_utils::GenericInferenceMap();
    for (auto &&[k, v] : map) { mapped_args[k] = v.Get(); }
    return FromMap(mapped_args);
}

spp::asts::GenericArgumentGroupAst::GenericArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Args(std::move(args)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::GenericArgumentGroupAst::~GenericArgumentGroupAst() = default;

auto spp::asts::GenericArgumentGroupAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::GenericArgumentGroupAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::GenericArgumentGroupAst::Clone() const
// Clone all the members of the ast.
    -> Unique<Ast> {
    return MakeUnique<GenericArgumentGroupAst>(
        AstClone(TokL), AstCloneVec(Args), AstClone(TokR));
}

auto spp::asts::GenericArgumentGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    if (not Args.IsEmpty()) {
        SPP_STRING_APPEND(TokL);
        SPP_STRING_EXTEND(Args, ", ");
        SPP_STRING_APPEND(TokR);
    }
    SPP_STRING_END;
}

auto spp::asts::GenericArgumentGroupAst::operator==(
    GenericArgumentGroupAst const &other) const
    -> bool {
    if (Args.Len() != other.Args.Len()) {
        return false;
    }

    for (std::size_t i = 0; i < Args.Len(); i++) {
        if (*Args[i] != *other.Args[i]) { return false; }
    }

    return true;
}

auto spp::asts::GenericArgumentGroupAst::operator+=(
    const GenericArgumentGroupAst &other)
    -> GenericArgumentGroupAst& {
    MergeGenerics(AstCloneVec(other.Args));
    return *this;
}

auto spp::asts::GenericArgumentGroupAst::operator+(
    const GenericArgumentGroupAst &other) const
    -> Unique<GenericArgumentGroupAst> {
    auto result = AstClone(this);
    *result += other;
    return result;
}

auto spp::asts::GenericArgumentGroupAst::Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    for (auto const &x : Args) {
        x->Stage4_QualifyTypes(sm, meta);
    }
}

auto spp::asts::GenericArgumentGroupAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppIdentifierDuplicateError;
    using analyse::errors::SppOrderInvalidError;
    using analyse::utils::order_utils::DoOrderArgs;

    // Check there are no duplicate type argument names.
    const auto type_arg_names = GetTypeKeywordArgs()
        | std::views::transform([](auto const &x) { return x->Name.Get(); })
        | spp::views::duplicates({}, spp::meta::deref)
        | std::ranges::to<Vec>();

    RaiseIf<SppIdentifierDuplicateError>(
        not type_arg_names.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*type_arg_names[0], *type_arg_names[1], "keyword generic type argument"));

    // Check there are no duplicate comp argument names.
    const auto comp_arg_names = GetCompKeywordArgs()
        | std::views::transform([](auto const &x) { return x->Name.Get(); })
        | spp::views::duplicates({}, spp::meta::deref)
        | std::ranges::to<Vec>();

    RaiseIf<SppIdentifierDuplicateError>(
        not comp_arg_names.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*comp_arg_names[0], *comp_arg_names[1], "keyword generic comp argument"));

    // Check the arguments are in the correct order.
    const auto unordered_args = DoOrderArgs(Args
        | spp::views::ptr
        | spp::views::cast_dynamic<mixins::OrderableAst*>
        | std::ranges::to<Vec>());

    RaiseIf<SppOrderInvalidError>(
        not unordered_args.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(unordered_args[0].First, *unordered_args[0].Second, unordered_args[1].First, *unordered_args[1].Second));

    // Analyse the arguments.
    for (auto const &x : Args) { x->Stage7_AnalyseSemantics(sm, meta); }
}

auto spp::asts::GenericArgumentGroupAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Check the arguments for memory issues.
    for (auto const &x : Args) { x->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::GenericArgumentGroupAst::TypeAt(
    const char *key) const
    -> GenericArgumentTypeAst const* {
    // Iterate the type arguments to find the matching key.
    for (const auto *arg : GetTypeKeywordArgs()) {
        if (arg->Name->TypeParts().Back()->Name == key) {
            return arg;
        }
    }
    return nullptr;
}

auto spp::asts::GenericArgumentGroupAst::CompAt(
    const char *key) const
    -> GenericArgumentCompAst const* {
    // Iterate the comptime arguments to find the matching key.
    for (const auto *arg : GetCompKeywordArgs()) {
        if (arg->Name->TypeParts().Back()->Name == key) {
            return arg;
        }
    }
    return nullptr;
}

auto spp::asts::GenericArgumentGroupAst::MergeGenerics(
    decltype(Args) &&other_args)
    -> void {
    // Append the other arguments to this argument group, checking named duplicates.
    for (auto &&other_arg : std::move(other_args)) {
        if (const auto kw_comp = other_arg->To<GenericArgumentCompKeywordAst>(); kw_comp != nullptr) {
            if (CompAt(kw_comp->Name->To<TypeIdentifierAst>()->Name.c_str()) != nullptr) { continue; }
            Args.EmplaceBack(std::move(other_arg));
        }
        else if (const auto kw_type = other_arg->To<GenericArgumentTypeKeywordAst>(); kw_type != nullptr) {
            if (TypeAt(kw_type->Name->To<TypeIdentifierAst>()->Name.c_str()) != nullptr) { continue; }
            Args.EmplaceBack(std::move(other_arg));
        }
    }
}

auto spp::asts::GenericArgumentGroupAst::GetTypeArgs() const
    -> Vec<GenericArgumentTypeAst*> {
    // Filter by casting.
    auto out = Vec<GenericArgumentTypeAst*>();
    for (auto const &arg : Args) {
        if (const auto type_arg = arg->To<GenericArgumentTypeAst>()) {
            out.EmplaceBack(type_arg);
        }
    }
    return out;
}

auto spp::asts::GenericArgumentGroupAst::GetCompArgs() const
    -> Vec<GenericArgumentCompAst*> {
    // Filter by casting.
    auto out = Vec<GenericArgumentCompAst*>();
    for (auto const &arg : Args) {
        if (const auto comp_arg = arg->To<GenericArgumentCompAst>()) {
            out.EmplaceBack(comp_arg);
        }
    }
    return out;
}

auto spp::asts::GenericArgumentGroupAst::GetKeywordArgs() const
    -> Vec<GenericArgumentAst*> {
    // Filter by casting.
    auto out = Vec<GenericArgumentAst*>();
    for (auto const &arg : Args) {
        if (const auto kw_t_arg = arg->To<GenericArgumentTypeKeywordAst>()) {
            out.EmplaceBack(kw_t_arg);
        }
        else if (const auto kw_c_arg = arg->To<GenericArgumentCompKeywordAst>()) {
            out.EmplaceBack(kw_c_arg);
        }
    }
    return out;
}

auto spp::asts::GenericArgumentGroupAst::GetPositionalArgs() const
    -> Vec<GenericArgumentAst*> {
    // Filter by casting.
    auto out = Vec<GenericArgumentAst*>();
    for (auto const &arg : Args) {
        if (const auto pos_t_arg = arg->To<GenericArgumentTypePositionalAst>()) {
            out.EmplaceBack(pos_t_arg);
        }
        else if (const auto pos_c_arg = arg->To<GenericArgumentCompPositionalAst>()) {
            out.EmplaceBack(pos_c_arg);
        }
    }
    return out;
}

auto spp::asts::GenericArgumentGroupAst::GetTypeKeywordArgs() const
    -> Vec<GenericArgumentTypeKeywordAst*> {
    // Filter by casting.
    auto out = Vec<GenericArgumentTypeKeywordAst*>();
    for (auto const &arg : Args) {
        if (const auto kw_t_arg = arg->To<GenericArgumentTypeKeywordAst>()) {
            out.EmplaceBack(kw_t_arg);
        }
    }
    return out;
}

auto spp::asts::GenericArgumentGroupAst::GetCompKeywordArgs() const
    -> Vec<GenericArgumentCompKeywordAst*> {
    // Filter by casting.
    auto out = Vec<GenericArgumentCompKeywordAst*>();
    for (auto const &arg : Args) {
        if (const auto kw_c_arg = arg->To<GenericArgumentCompKeywordAst>()) {
            out.EmplaceBack(kw_c_arg);
        }
    }
    return out;
}

auto spp::asts::GenericArgumentGroupAst::GetAllArgs() const
    -> Vec<GenericArgumentAst*> {
    // Convert args to raw pointers.
    auto out = Vec<GenericArgumentAst*>();
    for (auto const &arg : Args) {
        out.EmplaceBack(arg.Get());
    }
    return out;
}

SPP_MOD_END
