#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/order_utils.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_comp_keyword_ast.hpp>
#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_comp_ast.hpp>
#include <spp/asts/generic_parameter_type_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/mixins/orderable_ast.hpp>
#include <spp/pch.hpp>

#include <genex/views/cast.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/to.hpp>


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


auto spp::asts::GenericArgumentGroupAst::new_empty()
    -> std::unique_ptr<GenericArgumentGroupAst> {
    return std::make_unique<GenericArgumentGroupAst>(nullptr, decltype(args)(), nullptr);
}


auto spp::asts::GenericArgumentGroupAst::from_params(
    GenericParameterGroupAst const &generic_params)
    -> std::unique_ptr<GenericArgumentGroupAst> {
    // Create the list of arguments, initially empty.
    auto mapped_args = std::vector<std::unique_ptr<GenericArgumentAst>>();

    for (auto const &param : generic_params.params) {
        // Map type generic parameters to keyword type arguments.
        if (const auto type_param = ast_cast<GenericParameterTypeAst>(param.get())) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentTypeKeywordAst>(
                ast_clone(type_param->name), nullptr, ast_clone(type_param->name)));
        }

        // Map comptime generic parameters to keyword comptime arguments.
        else if (const auto comp_param = ast_cast<GenericParameterCompAst>(param.get())) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentCompKeywordAst>(
                ast_clone(comp_param->name), nullptr, IdentifierAst::from_type(*comp_param->name)));
        }
    }

    // Place the arguments into a group AST.
    return std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(mapped_args), nullptr);
}


auto spp::asts::GenericArgumentGroupAst::from_map(
    std::map<std::shared_ptr<TypeAst>, ExpressionAst const*> &&map)
    -> std::unique_ptr<GenericArgumentGroupAst> {
    // Create the list of arguments, initially empty.
    auto mapped_args = std::vector<std::unique_ptr<GenericArgumentAst>>();

    for (auto const &[arg_name, arg_val] : map) {
        // Map type ASTs to keyword type arguments.
        if (auto *arg_val_for_type = ast_cast<TypeAst>(arg_val)) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentTypeKeywordAst>(
                ast_clone(arg_name), nullptr, ast_clone(arg_val_for_type)));
        }

        // Map expression ASTs to keyword comptime arguments.
        else if (auto *arg_val_for_comp = ast_cast<ExpressionAst>(arg_val)) {
            mapped_args.emplace_back(std::make_unique<GenericArgumentCompKeywordAst>(
                ast_clone(arg_name), nullptr, ast_clone(arg_val_for_comp)));
        }
    }

    // Place the arguments into a group AST.
    return std::make_unique<GenericArgumentGroupAst>(nullptr, std::move(mapped_args), nullptr);
}


auto spp::asts::GenericArgumentGroupAst::type_at(
    const char *key) const
    -> GenericArgumentTypeAst const* {
    // Iterate the type arguments to find the matching key.
    for (const auto *arg : get_type_args() | genex::views::cast_dynamic<GenericArgumentTypeKeywordAst*>()) {
        if (asts::ast_cast<TypeIdentifierAst>(arg->name->type_parts().back())->name == key) {
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
        if (asts::ast_cast<TypeIdentifierAst>(arg->name->type_parts().back())->name == key) {
            return arg;
        }
    }
    return nullptr;
}


auto spp::asts::GenericArgumentGroupAst::get_type_args() const
    -> std::vector<GenericArgumentTypeAst*> {
    // Filter by casting.
    return args
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericArgumentTypeAst*>()
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericArgumentGroupAst::get_comp_args() const
    -> std::vector<GenericArgumentCompAst*> {
    // Filter by casting.
    return args
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericArgumentCompAst*>()
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericArgumentGroupAst::get_keyword_args() const
    -> std::vector<GenericArgumentAst*> {
    // Filter by casting.
    auto keyword_types = args
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericArgumentTypeKeywordAst*>()
        | genex::views::cast_dynamic<GenericArgumentAst*>()
        | genex::views::to<std::vector>();

    auto keyword_comps = args
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericArgumentCompKeywordAst*>()
        | genex::views::cast_dynamic<GenericArgumentAst*>()
        | genex::views::to<std::vector>();

    return genex::views::concat(keyword_types, keyword_comps)
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericArgumentGroupAst::get_positional_args() const
    -> std::vector<GenericArgumentAst*> {
    // Filter by casting.
    auto positional_types = args
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericArgumentTypePositionalAst*>()
        | genex::views::cast_dynamic<GenericArgumentAst*>()
        | genex::views::to<std::vector>();

    auto positional_comps = args
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericArgumentCompPositionalAst*>()
        | genex::views::cast_dynamic<GenericArgumentAst*>()
        | genex::views::to<std::vector>();

    return genex::views::concat(positional_types, positional_comps)
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericArgumentGroupAst::get_all_args() const
    -> std::vector<GenericArgumentAst*> {
    // Convert args to raw pointers.
    return args
        | genex::views::ptr
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericArgumentGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::GenericArgumentGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::GenericArgumentGroupAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenericArgumentGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(args),
        ast_clone(tok_r));
}


spp::asts::GenericArgumentGroupAst::operator std::string() const {
    SPP_STRING_START;
    if (not args.empty()) {
        SPP_STRING_APPEND(tok_l);
        SPP_STRING_EXTEND(args);
        SPP_STRING_APPEND(tok_r);
    }
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    if (not args.empty()) {
        SPP_PRINT_APPEND(tok_l);
        SPP_PRINT_EXTEND(args);
        SPP_PRINT_APPEND(tok_r);
    }
    SPP_PRINT_END;
}


auto spp::asts::GenericArgumentGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check there are no duplicate type argument names.
    const auto type_arg_names = get_keyword_args()
        | genex::views::cast_dynamic<GenericArgumentTypeKeywordAst*>()
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::views::materialize()
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not type_arg_names.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>().with_args(
            *type_arg_names[0], *type_arg_names[1], "keyword function-argument").with_scopes({sm->current_scope}).raise();
    }

    // Check there are no duplicate comp argument names.
    const auto comp_arg_names = get_keyword_args()
        | genex::views::cast_dynamic<GenericArgumentCompKeywordAst*>()
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::views::materialize()
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not comp_arg_names.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>().with_args(
            *comp_arg_names[0], *comp_arg_names[1], "keyword function-argument").with_scopes({sm->current_scope}).raise();
    }

    // Check the arguments are in the correct order.
    const auto unordered_args = analyse::utils::order_utils::order_args(args
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::views::to<std::vector>());

    if (not unordered_args.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppOrderInvalidError>().with_args(
            unordered_args[0].first, *unordered_args[0].second, unordered_args[1].first, *unordered_args[1].second).with_scopes({sm->current_scope}).raise();
    }

    // Analyse the arguments.
    args | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
}


auto spp::asts::GenericArgumentGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the arguments for memory issues.
    args | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}


auto operator==(
    spp::asts::GenericArgumentGroupAst const &lhs,
    spp::asts::GenericArgumentGroupAst const &rhs)
    -> bool {
    if (lhs.args.size() != rhs.args.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.args.size(); i++) {
        if (*lhs.args[i] != *rhs.args[i]) {
            return false;
        }
    }

    return true;
}
