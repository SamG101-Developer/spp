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
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import ankerl;
import genex;


spp::asts::GenericArgumentGroupAst::GenericArgumentGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(args) &&args,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    args(std::move(args)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}


spp::asts::GenericArgumentGroupAst::~GenericArgumentGroupAst() = default;


auto spp::asts::GenericArgumentGroupAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::GenericArgumentGroupAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::GenericArgumentGroupAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericArgumentGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(args),
        ast_clone(tok_r));
}


spp::asts::GenericArgumentGroupAst::operator std::string() const {
    SPP_STRING_START;
    if (not args.empty()) {
        SPP_STRING_APPEND(tok_l);
        SPP_STRING_EXTEND(args, ", ");
        SPP_STRING_APPEND(tok_r);
    }
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentGroupAst::new_empty()
    -> std::unique_ptr<GenericArgumentGroupAst> {
    return std::make_unique<GenericArgumentGroupAst>(
        nullptr, decltype(args)(), nullptr);
}


auto spp::asts::GenericArgumentGroupAst::from_params(
    GenericParameterGroupAst const &generic_params)
    -> std::unique_ptr<GenericArgumentGroupAst> {
    // Create the list of arguments, initially empty.
    auto mapped_args = std::vector<std::unique_ptr<GenericArgumentAst>>();

    for (auto const &param : generic_params.params) {
        // Map type generic parameters to keyword type arguments.
        if (const auto type_param = param->to<GenericParameterTypeAst>()) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentTypeKeywordAst>(
                ast_clone(type_param->name), nullptr, ast_clone(type_param->name)));
        }

        // Map comptime generic parameters to keyword comptime arguments.
        else if (const auto comp_param = param->to<GenericParameterCompAst>()) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentCompKeywordAst>(
                ast_clone(comp_param->name), nullptr, IdentifierAst::from_type(*comp_param->name)));
        }
    }

    // Place the arguments into a group AST.
    auto arg_group = new_empty();
    arg_group->args = std::move(mapped_args);
    return arg_group;
}


auto spp::asts::GenericArgumentGroupAst::from_map(
    analyse::utils::type_utils::GenericInferenceMap &&map)
    -> std::unique_ptr<GenericArgumentGroupAst> {
    // Create the list of arguments, initially empty.
    auto mapped_args = std::vector<std::unique_ptr<GenericArgumentAst>>();

    for (auto const &[arg_name, arg_val] : map) {
        // Map type ASTs to keyword type arguments.
        if (auto *arg_val_for_type = arg_val->to<TypeAst>()) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentTypeKeywordAst>(
                ast_clone(arg_name), nullptr, ast_clone(arg_val_for_type)));
        }

        // Map expression ASTs to keyword comptime arguments.
        else if (auto *arg_val_for_comp = arg_val->to<ExpressionAst>()) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentCompKeywordAst>(
                ast_clone(arg_name), nullptr, ast_clone(arg_val_for_comp)));
        }
    }

    // Place the arguments into a group AST.
    return std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(mapped_args), nullptr);
}


auto spp::asts::GenericArgumentGroupAst::from_map(
    ankerl::unordered_dense::map<std::shared_ptr<TypeIdentifierAst>, std::shared_ptr<const TypeAst>> &&map)
    -> std::unique_ptr<GenericArgumentGroupAst> {
    // Cast the values to "ExpressionAst const*".
    auto mapped_args = analyse::utils::type_utils::GenericInferenceMap();
    for (auto const &[k, v] : map) {
        mapped_args[k] = v.get();
    }
    return from_map(std::move(mapped_args));
}


auto spp::asts::GenericArgumentGroupAst::operator+=(
    const GenericArgumentGroupAst &other)
    -> GenericArgumentGroupAst& {
    merge_generics(ast_clone_vec(other.args));
    return *this;
}


auto spp::asts::GenericArgumentGroupAst::operator+(
    const GenericArgumentGroupAst &other) const
    -> std::unique_ptr<GenericArgumentGroupAst> {
    auto result = ast_clone(this);
    *result += other;
    return result;
}


auto spp::asts::GenericArgumentGroupAst::operator==(
    GenericArgumentGroupAst const &other) const
    -> bool {
    if (args.size() != other.args.size()) {
        return false;
    }

    for (std::size_t i = 0; i < args.size(); i++) {
        if (*args[i] != *other.args[i]) { return false; }
    }

    return true;
}


auto spp::asts::GenericArgumentGroupAst::type_at(
    const char *key) const
    -> GenericArgumentTypeAst const* {
    // Iterate the type arguments to find the matching key.
    for (const auto *arg : get_type_args() | genex::views::cast_dynamic<GenericArgumentTypeKeywordAst*>()) {
        if (std::dynamic_pointer_cast<TypeIdentifierAst>(arg->name->type_parts().back())->name == key) {
            return arg;
        }
    }
    return nullptr;
}


auto spp::asts::GenericArgumentGroupAst::comp_at(
    const char *key) const
    -> GenericArgumentCompAst const* {
    // Iterate the comptime arguments to find the matching key.
    for (const auto *arg : get_comp_args() | genex::views::cast_dynamic<GenericArgumentCompKeywordAst*>()) {
        if (std::dynamic_pointer_cast<TypeIdentifierAst>(arg->name->type_parts().back())->name == key) {
            return arg;
        }
    }
    return nullptr;
}


auto spp::asts::GenericArgumentGroupAst::merge_generics(
    decltype(args) &&other_args)
    -> void {
    // Append the other arguments to this argument group, checking named duplicates.
    for (auto &&other_arg : other_args) {
        if (const auto kw_comp = other_arg->to<GenericArgumentCompKeywordAst>(); kw_comp != nullptr) {
            if (comp_at(kw_comp->name->to<TypeIdentifierAst>()->name.c_str()) != nullptr) { continue; }
            args.emplace_back(ast_clone(other_arg));
        }
        else if (const auto kw_type = other_arg->to<GenericArgumentTypeKeywordAst>(); kw_type != nullptr) {
            if (type_at(kw_type->name->to<TypeIdentifierAst>()->name.c_str()) != nullptr) { continue; }
            args.emplace_back(ast_clone(other_arg));
        }
    }
}


auto spp::asts::GenericArgumentGroupAst::get_type_args() const
    -> std::vector<GenericArgumentTypeAst*> {
    // Filter by casting.
    auto out = std::vector<GenericArgumentTypeAst*>();
    for (auto const &arg : args) {
        if (const auto type_arg = arg->to<GenericArgumentTypeAst>()) {
            out.emplace_back(type_arg);
        }
    }
    return out;
}


auto spp::asts::GenericArgumentGroupAst::get_comp_args() const
    -> std::vector<GenericArgumentCompAst*> {
    // Filter by casting.
    auto out = std::vector<GenericArgumentCompAst*>();
    for (auto const &arg : args) {
        if (const auto comp_arg = arg->to<GenericArgumentCompAst>()) {
            out.emplace_back(comp_arg);
        }
    }
    return out;
}


auto spp::asts::GenericArgumentGroupAst::get_keyword_args() const
    -> std::vector<GenericArgumentAst*> {
    // Filter by casting.
    auto out = std::vector<GenericArgumentAst*>();
    for (auto const &arg : args) {
        if (const auto kw_t_arg = arg->to<GenericArgumentTypeKeywordAst>()) {
            out.emplace_back(kw_t_arg);
        }
        else if (const auto kw_c_arg = arg->to<GenericArgumentCompKeywordAst>()) {
            out.emplace_back(kw_c_arg);
        }
    }
    return out;
}


auto spp::asts::GenericArgumentGroupAst::get_positional_args() const
    -> std::vector<GenericArgumentAst*> {
    // Filter by casting.
    auto out = std::vector<GenericArgumentAst*>();
    for (auto const &arg : args) {
        if (const auto pos_t_arg = arg->to<GenericArgumentTypePositionalAst>()) {
            out.emplace_back(pos_t_arg);
        }
        else if (const auto pos_c_arg = arg->to<GenericArgumentCompPositionalAst>()) {
            out.emplace_back(pos_c_arg);
        }
    }
    return out;
}


auto spp::asts::GenericArgumentGroupAst::get_type_keyword_args() const
    -> std::vector<GenericArgumentTypeKeywordAst*> {
    // Filter by casting.
    auto out = std::vector<GenericArgumentTypeKeywordAst*>();
    for (auto const &arg : args) {
        if (const auto kw_t_arg = arg->to<GenericArgumentTypeKeywordAst>()) {
            out.emplace_back(kw_t_arg);
        }
    }
    return out;
}


auto spp::asts::GenericArgumentGroupAst::get_comp_keyword_args() const
    -> std::vector<GenericArgumentCompKeywordAst*> {
    // Filter by casting.
    auto out = std::vector<GenericArgumentCompKeywordAst*>();
    for (auto const &arg : args) {
        if (const auto kw_c_arg = arg->to<GenericArgumentCompKeywordAst>()) {
            out.emplace_back(kw_c_arg);
        }
    }
    return out;
}


auto spp::asts::GenericArgumentGroupAst::get_all_args() const
    -> std::vector<GenericArgumentAst*> {
    // Convert args to raw pointers.
    auto out = std::vector<GenericArgumentAst*>();
    for (auto const &arg : args) {
        out.emplace_back(arg.get());
    }
    return out;
}


auto spp::asts::GenericArgumentGroupAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    for (auto const &x : args) {
        x->stage_4_qualify_types(sm, meta);
    }
}


auto spp::asts::GenericArgumentGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check there are no duplicate type argument names.
    const auto type_arg_names = get_keyword_args()
        | genex::views::cast_dynamic<GenericArgumentTypeKeywordAst*>()
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::to<std::vector>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    raise_if<analyse::errors::SppIdentifierDuplicateError>(
        not type_arg_names.empty(),
        {sm->current_scope}, ERR_ARGS(*type_arg_names[0], *type_arg_names[1], "keyword function-argument"));

    // Check there are no duplicate comp argument names.
    const auto comp_arg_names = get_keyword_args()
        | genex::views::cast_dynamic<GenericArgumentCompKeywordAst*>()
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::to<std::vector>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    raise_if<analyse::errors::SppIdentifierDuplicateError>(
        not comp_arg_names.empty(),
        {sm->current_scope}, ERR_ARGS(*comp_arg_names[0], *comp_arg_names[1], "keyword function-argument"));

    // Check the arguments are in the correct order.
    const auto unordered_args = analyse::utils::order_utils::order_args(args
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::to<std::vector>());

    raise_if<analyse::errors::SppOrderInvalidError>(
        not unordered_args.empty(),
        {sm->current_scope}, ERR_ARGS(unordered_args[0].first, *unordered_args[0].second, unordered_args[1].first, *unordered_args[1].second));

    // Analyse the arguments.
    for (auto const &x : args) {
        x->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::GenericArgumentGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the arguments for memory issues.
    for (auto const &x : args) {
        x->stage_8_check_memory(sm, meta);
    }
}
