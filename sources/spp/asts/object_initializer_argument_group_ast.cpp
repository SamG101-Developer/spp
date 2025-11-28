module;
#include <genex/to_container.hpp>
#include <genex/operations/access.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/intersperse.hpp>
#include <genex/views/join.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/remove.hpp>
#include <genex/views/set_algorithms.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.object_initializer_argument_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.class_attribute_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.lex.tokens;


spp::asts::ObjectInitializerArgumentGroupAst::ObjectInitializerArgumentGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(args) &&args,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    args(std::move(args)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}


spp::asts::ObjectInitializerArgumentGroupAst::~ObjectInitializerArgumentGroupAst() = default;


auto spp::asts::ObjectInitializerArgumentGroupAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ObjectInitializerArgumentGroupAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::ObjectInitializerArgumentGroupAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ObjectInitializerArgumentGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(args),
        ast_clone(tok_r));
}


spp::asts::ObjectInitializerArgumentGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(args);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ObjectInitializerArgumentGroupAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(args);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::ObjectInitializerArgumentGroupAst::new_empty()
    -> std::unique_ptr<ObjectInitializerArgumentGroupAst> {
    return std::make_unique<ObjectInitializerArgumentGroupAst>(
        nullptr, decltype(args)(), nullptr);
}


auto spp::asts::ObjectInitializerArgumentGroupAst::get_autofill_arg()
    -> ObjectInitializerArgumentShorthandAst* {
    // Get the first shorthand argument tagged with the ".." token.
    const auto filtered = args
        | genex::views::ptr
        | genex::views::cast_dynamic<ObjectInitializerArgumentShorthandAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr and x->tok_ellipsis != nullptr; })
        | genex::to<std::vector>();
    return filtered.empty() ? nullptr : filtered[0];
}


auto spp::asts::ObjectInitializerArgumentGroupAst::get_non_autofill_args()
    -> std::vector<ObjectInitializerArgumentAst*> {
    // Get the first shorthand argument tagged with the ".." token.
    return args
        | genex::views::ptr
        | genex::views::remove(get_autofill_arg())
        | genex::to<std::vector>();
}


auto spp::asts::ObjectInitializerArgumentGroupAst::get_shorthand_args()
    -> std::vector<ObjectInitializerArgumentShorthandAst*> {
    // Get all shorthand arguments tagged with the ".." token.
    return args
        | genex::views::ptr
        | genex::views::cast_dynamic<ObjectInitializerArgumentShorthandAst*>()
        | genex::to<std::vector>();
}


auto spp::asts::ObjectInitializerArgumentGroupAst::get_keyword_args()
    -> std::vector<ObjectInitializerArgumentKeywordAst*> {
    // Get all keyword arguments.
    return args
        | genex::views::ptr
        | genex::views::cast_dynamic<ObjectInitializerArgumentKeywordAst*>()
        | genex::to<std::vector>();
}


auto spp::asts::ObjectInitializerArgumentGroupAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Ensure there are no duplicate member names. This needs to be done before semantic analysis as other ASTs might
    // try reading a duplicate attribute before an error is raised.
    const auto duplicates = get_keyword_args()
        | genex::views::transform([](auto &&x) { return x->name; })
        | genex::views::materialize
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    if (not duplicates.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>().with_args(
            *duplicates[0], *duplicates[1], "keyword object initializer arguments").with_scopes({sm->current_scope}).raise();
    }

    // Get the attributes on the type and supertypes.
    const auto all_attrs = analyse::utils::type_utils::get_all_attrs(*meta->object_init_type, sm);

    // Analyse the arguments in the group.
    for (auto &&arg : args) {
        // Return type overload helper.
        meta->save();
        if (const auto kw_arg = ast_cast<ObjectInitializerArgumentKeywordAst>(arg.get()); kw_arg != nullptr) {
            SPP_RETURN_TYPE_OVERLOAD_HELPER(arg->val.get())
            {
                // Multiple attributes with same name (via base classes) -> can't infer the one to use.
                auto attrs = all_attrs
                    | genex::views::filter([kw_arg](auto &&x) { return *x.first->name == *kw_arg->name; })
                    | genex::to<std::vector>();
                if (attrs.size() > 1) { continue; }

                // Use the type off the single matching attribute.
                const auto attr_type_sym = attrs[0].second->get_type_symbol(attrs[0].first->type);
                const auto attr_type = attr_type_sym->is_generic ? nullptr : attr_type_sym->fq_name();
                meta->return_type_overload_resolver_type = std::move(attr_type);
            }
        }

        arg->stage_7_analyse_semantics(sm, meta);
        meta->restore();
    }
}


auto spp::asts::ObjectInitializerArgumentGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the attributes on the type and supertypes.
    const auto cls_sym = sm->current_scope->get_type_symbol(meta->object_init_type);
    const auto all_attrs = analyse::utils::type_utils::get_all_attrs(*meta->object_init_type, sm);
    const auto all_attr_names = all_attrs
        | genex::views::transform([](auto &&x) { return x.first->name; })
        | genex::to<std::vector>();

    // Check there is at most 1 autofill argument.
    const auto af_args = get_shorthand_args()
        | genex::views::filter([](auto &&x) { return x->tok_ellipsis != nullptr; })
        | genex::to<std::vector>();

    if (af_args.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError>().with_args(
            *af_args[0], *af_args[1]).with_scopes({sm->current_scope}).raise();
    }

    // Check there are no invalidly named arguments.
    auto arg_names = get_non_autofill_args()
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::to<std::vector>();

    const auto invalid_args = arg_names
        | genex::views::set_difference_unsorted(all_attr_names, SPP_INSTANT_INDIRECT, SPP_INSTANT_INDIRECT)
        | genex::to<std::vector>();

    if (not invalid_args.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppArgumentNameInvalidError>().with_args(
            *meta->object_init_type, "attribute", *invalid_args[0], "object initializer argument").with_scopes({sm->current_scope}).raise();
    }

    // Type check the non-autofill arguments against the class attributes.
    for (auto &&arg : get_non_autofill_args()) {
        auto matching_attrs = all_attrs
            | genex::views::filter([&arg](auto const &x) { return *x.first->name == *arg->name; })
            | genex::to<std::vector>();

        if (matching_attrs.size() > 1) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppAmbiguousMemberAccessError>().with_args(
                *std::get<0>(matching_attrs[0]), *std::get<0>(matching_attrs[1]), *this).with_scopes({sm->current_scope}).raise();
        }

        auto [attr, scope] = matching_attrs[0];
        const auto attr_type = scope->get_type_symbol(cls_sym->scope->get_var_symbol(attr->name)->type)->fq_name();
        meta->save();
        meta->assignment_target_type = attr_type;
        auto arg_type = arg->infer_type(sm, meta);
        meta->restore();

        if (not analyse::utils::type_utils::symbolic_eq(*attr_type, *arg_type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *attr, *attr_type, *arg, *arg_type).with_scopes({sm->current_scope}).raise();
        }
    }

    // Type check the default argument (if it exists).
    if (const auto af_arg = get_autofill_arg(); af_arg != nullptr) {
        const auto af_arg_type = af_arg->val->infer_type(sm, meta);
        if (not analyse::utils::type_utils::symbolic_eq(*af_arg_type, *meta->object_init_type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *meta->object_init_type, *meta->object_init_type, *af_arg, *af_arg_type).with_scopes({sm->current_scope}).raise();
        }
    }
}


auto spp::asts::ObjectInitializerArgumentGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the arguments.
    for (auto &&arg : args) {
        arg->stage_8_check_memory(sm, meta);
    }
}
