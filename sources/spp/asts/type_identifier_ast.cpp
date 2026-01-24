module;
#include <spp/macros.hpp>

module spp.asts.type_identifier_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
import spp.asts.class_prototype_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;

import absl;
import genex;


// int count = 0;


spp::asts::TypeIdentifierAst::TypeIdentifierAst(
    const std::size_t pos,
    decltype(name) &&name,
    decltype(generic_arg_group) generic_arg_group) :
    name(std::move(name)),
    generic_arg_group(std::move(generic_arg_group)),
    m_pos(pos),
    m_is_never_type(false) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_arg_group);
}


auto spp::asts::TypeIdentifierAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    // Double dispatch to the appropriate equals method.
    return other.equals_type_identifier(*this);
}


auto spp::asts::TypeIdentifierAst::equals_type_identifier(
    TypeIdentifierAst const &other) const
    -> std::strong_ordering {
    // Check the name and args are equal.
    if (name == other.name and *generic_arg_group == *other.generic_arg_group) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::TypeIdentifierAst::pos_start() const
    -> std::size_t {
    return m_pos;
}


auto spp::asts::TypeIdentifierAst::pos_end() const
    -> std::size_t {
    return generic_arg_group ? generic_arg_group->pos_end() : m_pos + name.length();
}


auto spp::asts::TypeIdentifierAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<TypeIdentifierAst>(
        m_pos,
        std::string(name),
        ast_clone(generic_arg_group));
}


spp::asts::TypeIdentifierAst::operator std::string() const {
    SPP_STRING_START;
    raw_string.append(name);
    SPP_STRING_APPEND(generic_arg_group);
    SPP_STRING_END;
}


auto spp::asts::TypeIdentifierAst::from_identifier(
    IdentifierAst const &identifier)
    -> std::shared_ptr<TypeIdentifierAst> {
    return std::make_shared<TypeIdentifierAst>(identifier.pos_start(), std::string(identifier.val), nullptr);
}


auto spp::asts::TypeIdentifierAst::from_string(
    std::string const &identifier)
    -> std::shared_ptr<TypeIdentifierAst> {
    return std::make_shared<TypeIdentifierAst>(0, std::string(identifier), nullptr);
}


auto spp::asts::TypeIdentifierAst::iterator() const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    // First yield is the original type being iterated over.
    auto parts = std::vector<std::shared_ptr<const TypeIdentifierAst>>{};
    parts.emplace_back(std::dynamic_pointer_cast<const TypeIdentifierAst>(shared_from_this()));

    for (auto &&g : generic_arg_group->args) {
        // Positional generic comp argument with identifier value.
        if (auto &&comp_positional_arg = g->to<GenericArgumentCompPositionalAst>()) {
            if (auto &&ident_val = comp_positional_arg->val->to<IdentifierAst>()) {
                parts.emplace_back(from_identifier(*ident_val));
            }
        }

        // Keyword generic comp argument with identifier value.
        else if (auto &&comp_keyword_arg = g->to<GenericArgumentCompKeywordAst>()) {
            if (auto &&ident_val = comp_keyword_arg->val->to<IdentifierAst>()) {
                parts.emplace_back(from_identifier(*ident_val));
            }
        }

        // Positional generic type arguments => recursive iteration.
        else if (auto &&type_positional_arg = g->to<GenericArgumentTypePositionalAst>()) {
            for (auto &&ti : type_positional_arg->val->iterator()) {
                parts.emplace_back(ti);
            }
        }

        // Keyword generic type arguments => recursive iteration.
        else if (auto &&type_keyword_arg = g->to<GenericArgumentTypeKeywordAst>()) {
            for (auto &&ti : type_keyword_arg->val->iterator()) {
                parts.emplace_back(ti);
            }
        }
    }

    return parts;
}


auto spp::asts::TypeIdentifierAst::is_never_type() const
    -> bool {
    return m_is_never_type;
}


auto spp::asts::TypeIdentifierAst::ns_parts() const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    return {};
}


auto spp::asts::TypeIdentifierAst::ns_parts()
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {};
}


auto spp::asts::TypeIdentifierAst::type_parts() const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    return std::vector{std::dynamic_pointer_cast<const TypeIdentifierAst>(shared_from_this())};
}


auto spp::asts::TypeIdentifierAst::type_parts()
    -> std::vector<std::shared_ptr<TypeIdentifierAst>> {
    return std::vector{std::dynamic_pointer_cast<TypeIdentifierAst>(shared_from_this())};
}


auto spp::asts::TypeIdentifierAst::without_convention() const
    -> std::shared_ptr<const TypeAst> {
    return shared_from_this();
}


auto spp::asts::TypeIdentifierAst::get_convention() const
    -> ConventionAst* {
    return nullptr;
}


auto spp::asts::TypeIdentifierAst::with_convention(
    std::unique_ptr<ConventionAst> &&conv) const
    -> std::shared_ptr<TypeAst> {
    if (conv == nullptr) { return ast_clone(this); }

    auto borrow_op = std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv));
    auto wrapped = std::make_shared<TypeUnaryExpressionAst>(std::move(borrow_op), ast_clone(this));
    return wrapped;
}


auto spp::asts::TypeIdentifierAst::without_generics() const
    -> std::shared_ptr<TypeAst> {
    return std::make_shared<TypeIdentifierAst>(m_pos, std::string(name), nullptr);
}


auto spp::asts::TypeIdentifierAst::substitute_generics(
    std::vector<GenericArgumentAst*> const &args) const
    -> std::shared_ptr<TypeAst> {
    auto name_clone = ast_clone(this);
    if (args.empty() or generic_arg_group == nullptr) { return name_clone; }

    // Get the generic type arguments.
    auto gen_type_args = args
        | genex::views::cast_dynamic<GenericArgumentTypeKeywordAst*>()
        | genex::views::transform([](auto &&g) { return std::make_pair(g->name, g->val->template to<ExpressionAst>()); })
        | genex::to<std::vector>();

    // Get the generic comp arguments.
    auto gen_comp_args = args
        | genex::views::cast_dynamic<GenericArgumentCompKeywordAst*>()
        | genex::views::transform([](auto &&g) { return std::make_pair(g->name, g->val.get()); })
        | genex::to<std::vector>();

    // Check if this type directly matches any generic type argument name.
    for (auto const &[gen_arg_name, gen_arg_val] : gen_type_args) {
        if (*this == *gen_arg_name->to<TypeIdentifierAst>()) {
            return ast_clone(gen_arg_val->to<TypeAst>());
        }
    }

    // Substitute generics in the comp arguments' types.
    gen_type_args.append_range(gen_comp_args);
    for (auto const &[gen_arg_name, gen_arg_val] : gen_comp_args) {
        for (auto const &g : name_clone->generic_arg_group->get_comp_args()) {
            if (auto const *ident_val = g->val->to<IdentifierAst>(); ident_val != nullptr and *ident_val == *IdentifierAst::from_type(*gen_arg_name)) {
                g->val = ast_clone(gen_arg_val);
            }
        }
    }

    // Substitute generics in the type arguments' types.
    for (auto const &g : name_clone->generic_arg_group->get_type_args()) {
        g->val = g->val->substitute_generics(args);
    }

    // Return the cloned type with generics substituted.
    return name_clone;
}


auto spp::asts::TypeIdentifierAst::contains_generic(
    GenericParameterAst const &generic) const
    -> bool {
    // Check if the parameter's name is in the type parts iterated from this type.
    auto cast_name = generic.name->to<TypeIdentifierAst>();
    return genex::any_of(
        iterator() | genex::to<std::vector>(), [&cast_name](auto ti) { return *ti == *cast_name; });
}


auto spp::asts::TypeIdentifierAst::with_generics(
    std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const
    -> std::shared_ptr<TypeAst> {
    // Attach the new generic argument group to a clone of this type identifier.
    return std::make_shared<TypeIdentifierAst>(m_pos, std::string(name), std::move(arg_group));
}


auto spp::asts::TypeIdentifierAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the generic argument types.
    // meta->save();
    // meta->type_analysis_type_scope = sm->current_scope->get_type_symbol(without_generics())->scope;
    for (auto &&g : generic_arg_group->get_type_args()) {
        g->stage_4_qualify_types(sm, meta);
    }
    // meta->restore();
}


auto spp::asts::TypeIdentifierAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // count += 1;
    // std::cout << "Analysing TypeIdentifierAst: " << to_string() << " (" << count << ")" << std::endl;

    // Determine the scope and get the type symbol.
    const auto scope = meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope;
    const auto type_sym = analyse::utils::type_utils::get_type_sym_or_error(
        *scope, *std::dynamic_pointer_cast<TypeIdentifierAst>(without_generics()), *sm, meta);
    // const auto type_scope = type_sym->scope;
    if (type_sym->is_generic or name == "Self") { return; }

    // Name all the generic arguments.
    const auto is_tuple = ( {
        const auto as_unary = std::dynamic_pointer_cast<TypeUnaryExpressionAst>(type_sym->fq_name()->without_generics());
        as_unary != nullptr and *as_unary == *generate::common_types_precompiled::TUP->to<TypeUnaryExpressionAst>();
    });

    analyse::utils::func_utils::name_gn_args(
        *generic_arg_group,
        *(type_sym->alias_stmt ? type_sym->alias_stmt->generic_param_group : type_sym->type->generic_param_group),
        *this, *sm, *meta, is_tuple);

    // Stop here is there is a flag to not check generics.
    if (meta->skip_type_analysis_generic_checks) {
        return;
    }

    // Analyse the generic arguments.
    meta->type_analysis_type_scope = nullptr;
    generic_arg_group->stage_7_analyse_semantics(sm, meta);

    // Infer the generic arguments from information given from object initialization.
    const auto owner = analyse::utils::type_utils::get_type_sym_or_error(
        *scope, *without_generics()->to<TypeIdentifierAst>(), *sm, meta)->fq_name();
    const auto owner_sym = sm->current_scope->get_type_symbol(owner);

    analyse::utils::func_utils::infer_gn_args(
        *generic_arg_group,
        *(type_sym->alias_stmt ? type_sym->alias_stmt->generic_param_group : type_sym->type->generic_param_group),
        generic_arg_group->get_all_args(),
        meta->infer_source, meta->infer_target,
        owner, *(owner_sym != nullptr ? owner_sym->scope : type_sym->scope),
        nullptr, is_tuple, *sm, *meta);
    generic_arg_group->stage_7_analyse_semantics(sm, meta);

    // For variant types, collapse any duplicate generic arguments.
    // if (analyse::utils::type_utils::symbolic_eq(*without_generics(), *generate::common_types_precompiled::VAR, *scope, *sm->current_scope, false)) {
    //     auto inner_types = analyse::utils::type_utils::deduplicate_variant_inner_types(*this, *sm->current_scope);
    //     auto inner_types_as_tup = generate::common_types::tuple_type(pos_start(), std::move(inner_types));
    //     meta->save();
    //     meta->type_analysis_type_scope = type_scope;
    //     inner_types_as_tup->stage_7_analyse_semantics(sm, meta);
    //     meta->restore();
    //     ast_cast<GenericArgumentTypeAst>(generic_arg_group->args[0].get())->val = std::move(inner_types_as_tup);
    // }

    // If the generically filled type doesn't exist (Vec[Str]), but the base does (Vec[T]), create it.
    if (not scope->has_type_symbol(shared_from_this())) {
        const auto external_generics = sm->current_scope->get_extended_generic_symbols(generic_arg_group->get_all_args(), meta->ignore_cmp_generic);
        analyse::utils::type_utils::create_generic_cls_scope(*this, *type_sym, external_generics, is_tuple, sm, meta);
    }
}


auto spp::asts::TypeIdentifierAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Fully qualify this type name from the scope.
    const auto type_scope = meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope;
    const auto type_sym = type_scope->get_type_symbol(shared_from_this());
    return type_sym->fq_name();
}


auto spp::asts::TypeIdentifierAst::ankerl_hash() const
    -> std::size_t {
    // Hash based on the name only.
    return absl::Hash<std::string>()(name);
}
