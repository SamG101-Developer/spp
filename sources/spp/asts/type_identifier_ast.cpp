#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/func_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/generic_argument_comp_keyword_ast.hpp>
#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>
#include <spp/asts/type_unary_expression_ast.hpp>
#include <spp/asts/type_unary_expression_operator_borrow_ast.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>

#include <genex/algorithms/any_of.hpp>
#include <genex/operations/cmp.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/ptr.hpp>


spp::asts::TypeIdentifierAst::TypeIdentifierAst(
    const std::size_t pos,
    decltype(name) &&name,
    decltype(generic_arg_group) generic_arg_group) :
    name(std::move(name)),
    generic_arg_group(std::move(generic_arg_group)),
    m_pos(pos),
    m_is_never_type(false) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_arg_group);
    this->generic_arg_group->tok_l->m_pos = this->generic_arg_group->tok_l->m_pos ? this->generic_arg_group->tok_l->m_pos : pos + this->pos_start() + this->name.length();
    this->generic_arg_group->tok_r->m_pos = this->generic_arg_group->tok_r->m_pos ? this->generic_arg_group->tok_r->m_pos : this->generic_arg_group->tok_l->m_pos + 1;
}


spp::asts::TypeIdentifierAst::~TypeIdentifierAst() = default;


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


auto spp::asts::TypeIdentifierAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    formatted_string.append(name);
    SPP_PRINT_APPEND(generic_arg_group);
    SPP_PRINT_END;
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
    -> genex::generator<std::shared_ptr<const TypeIdentifierAst>> {
    // First yield is the original type being iterated over.
    co_yield std::dynamic_pointer_cast<const TypeIdentifierAst>(shared_from_this());

    for (auto &&g : generic_arg_group->args) {
        // Positional generic comp argument with identifier value.
        if (auto &&comp_positional_arg = ast_cast<GenericArgumentCompPositionalAst>(g.get())) {
            if (auto &&ident_val = ast_cast<IdentifierAst>(comp_positional_arg->val.get())) {
                co_yield from_identifier(*ident_val);
            }
        }

        // Keyword generic comp argument with identifier value.
        else if (auto &&comp_keyword_arg = ast_cast<GenericArgumentCompKeywordAst>(g.get())) {
            if (auto &&ident_val = ast_cast<IdentifierAst>(comp_keyword_arg->val.get())) {
                co_yield from_identifier(*ident_val);
            }
        }

        // Positional generic type arguments => recursive iteration.
        else if (auto &&type_positional_arg = ast_cast<GenericArgumentTypePositionalAst>(g.get())) {
            for (auto &&ti : type_positional_arg->val->iterator()) {
                co_yield ti;
            }
        }

        // Keyword generic type arguments => recursive iteration.
        else if (auto &&type_keyword_arg = ast_cast<GenericArgumentTypeKeywordAst>(g.get())) {
            for (auto &&ti : type_keyword_arg->val->iterator()) {
                co_yield ti;
            }
        }
    }
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
    if (conv == nullptr) {
        return ast_clone(this);
    }

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

    // Get the generic type arguments.
    auto gen_type_args = args
        | genex::views::cast_dynamic<GenericArgumentTypeKeywordAst*>()
        | genex::views::transform([](auto &&g) { return std::make_pair(g->name, ast_cast<ExpressionAst>(g->val.get())); })
        | genex::to<std::vector>();

    // Get the generic comp arguments.
    auto gen_comp_args = args
        | genex::views::cast_dynamic<GenericArgumentCompKeywordAst*>()
        | genex::views::transform([](auto &&g) { return std::make_pair(g->name, g->val.get()); })
        | genex::to<std::vector>();

    // Check if this type directly matches any generic type argument name.
    for (auto &&[gen_arg_name, gen_arg_val] : gen_type_args) {
        if (*this == *ast_cast<TypeIdentifierAst>(gen_arg_name.get())) {
            return ast_clone(ast_cast<TypeAst>(gen_arg_val));
        }
    }

    // Substitute generics in the comp arguments' types.
    for (auto &&[gen_arg_name, gen_arg_val] : genex::views::concat(gen_type_args, gen_comp_args)) {
        for (auto const &g : name_clone->generic_arg_group->get_comp_args()) {
            if (auto const *ident_val = ast_cast<IdentifierAst>(g->val.get()); ident_val != nullptr and *ident_val == *IdentifierAst::from_type(*gen_arg_name)) {
                g->val = ast_clone(gen_arg_val);
            }
        }
    }

    // Substitute generics in the type arguments' types.
    for (auto const &g : name_clone->generic_arg_group->get_type_args() | genex::views::cast_dynamic<GenericArgumentTypeAst*>()) {
        g->val = g->val->substitute_generics(args);
    }

    // Return the cloned type with generics substituted.
    return name_clone;
}


auto spp::asts::TypeIdentifierAst::contains_generic(
    GenericParameterAst const &generic) const
    -> bool {
    // Check if the parameter's name is in the type parts iterated from this type.
    auto cast_name = ast_cast<TypeIdentifierAst>(generic.name.get());
    return genex::algorithms::any_of(
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
    mixins::CompilerMetaData *meta)
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
    mixins::CompilerMetaData *meta)
    -> void {
    // Determine the scope and get the type symbol.
    const auto scope = meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope;
    const auto type_sym = analyse::utils::type_utils::get_type_part_symbol_with_error(
        *scope, *std::dynamic_pointer_cast<TypeIdentifierAst>(without_generics()), *sm, meta);
    // const auto type_scope = type_sym->scope;
    if (type_sym->is_generic or name == "Self") { return; }

    // Name all the generic arguments.
    const auto is_tuple = ( {
        const auto as_unary = std::dynamic_pointer_cast<TypeUnaryExpressionAst>(type_sym->fq_name()->without_generics());
        as_unary != nullptr and *as_unary == ast_cast<TypeUnaryExpressionAst>(*generate::common_types_precompiled::TUP);
    });

    analyse::utils::func_utils::name_generic_args(
        generic_arg_group->args,
        type_sym->alias_stmt ? type_sym->alias_stmt->generic_param_group->get_all_params() : type_sym->type->generic_param_group->get_all_params(),
        *this, *sm, meta, is_tuple);

    // Stop here is there is a flag to not check generics.
    if (meta->skip_type_analysis_generic_checks) {
        return;
    }

    // Analyse the generic arguments.
    meta->type_analysis_type_scope = nullptr;
    generic_arg_group->stage_7_analyse_semantics(sm, meta);

    // Infer the generic arguments from information given from object initialization.
    const auto owner = analyse::utils::type_utils::get_type_part_symbol_with_error(
        *scope, ast_cast<TypeIdentifierAst>(*without_generics()), *sm, meta)->fq_name();
    const auto owner_sym = sm->current_scope->get_type_symbol(owner);

    analyse::utils::func_utils::infer_generic_args(
        generic_arg_group->args,
        type_sym->alias_stmt ? type_sym->alias_stmt->generic_param_group->get_all_params() : type_sym->type->generic_param_group->get_all_params(),
        type_sym->alias_stmt ? type_sym->alias_stmt->generic_param_group->get_optional_params() : type_sym->type->generic_param_group->get_optional_params(),
        generic_arg_group->get_all_args(),
        meta->infer_source, meta->infer_target,
        owner, owner_sym != nullptr ? owner_sym->scope : nullptr, nullptr, is_tuple, *sm, meta);
    meta->infer_source = {};
    meta->infer_target = {};
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
        const auto external_generics = sm->current_scope->get_extended_generic_symbols(generic_arg_group->get_all_args());
        analyse::utils::type_utils::create_generic_cls_scope(*this, *type_sym, external_generics, is_tuple, sm, meta);
    }
}


auto spp::asts::TypeIdentifierAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Fully qualify this type name from the scope.
    const auto type_scope = meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope;
    const auto type_sym = type_scope->get_type_symbol(shared_from_this());
    return type_sym->fq_name();
}
