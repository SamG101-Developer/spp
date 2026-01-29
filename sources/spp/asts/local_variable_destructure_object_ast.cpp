module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.local_variable_destructure_object_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.type_ast;
import spp.lex.tokens;
import genex;


spp::asts::LocalVariableDestructureObjectAst::LocalVariableDestructureObjectAst(
    decltype(type) &&type,
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    type(std::move(type)),
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}


spp::asts::LocalVariableDestructureObjectAst::~LocalVariableDestructureObjectAst() = default;


auto spp::asts::LocalVariableDestructureObjectAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::LocalVariableDestructureObjectAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::LocalVariableDestructureObjectAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableDestructureObjectAst>(
        ast_clone(type),
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::LocalVariableDestructureObjectAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems, ", ");
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureObjectAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return std::make_shared<IdentifierAst>(pos_start(), "_UNMATCHABLE");
}


auto spp::asts::LocalVariableDestructureObjectAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return elems
        | genex::views::transform(&LocalVariableAst::extract_names)
        | genex::views::join
        | genex::to<std::vector>();
}


auto spp::asts::LocalVariableDestructureObjectAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {

    // Get the value and analyse it and the type.
    const auto val = meta->let_stmt_value;
    const auto val_type = val->infer_type(sm, meta);
    type->stage_7_analyse_semantics(sm, meta);

    const auto cls_proto = sm->current_scope->get_type_symbol(type)->type;
    const auto cls_attrs = cls_proto != nullptr ? cls_proto->impl->members | genex::views::ptr | genex::to<std::vector>() : std::vector<ClassMemberAst*>{};

    const auto attributes = cls_attrs
        | genex::views::cast_dynamic<ClassAttributeAst*>()
        | genex::to<std::vector>();

    const auto multi_arg_skips = elems
        | genex::views::ptr
        | genex::views::cast_dynamic<LocalVariableDestructureSkipMultipleArgumentsAst*>()
        | genex::to<std::vector>();

    const auto assigned_attributes = elems
        | genex::views::ptr
        | genex::views::filter([](auto const &x) { return x->template to<LocalVariableDestructureSkipMultipleArgumentsAst>() == nullptr; })
        | genex::views::transform([](auto const &x) { return x->extract_name(); })
        | genex::to<std::vector>();

    const auto missing_attributes = attributes
        | genex::views::transform([](auto const &x) { return x->name; })
        | genex::to<std::vector>()
        | genex::views::not_in(assigned_attributes, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();

    // Check the type matches.
    raise_if<analyse::errors::SppTypeMismatchError>(
        not analyse::utils::type_utils::symbolic_eq(*val_type, *type, *sm->current_scope, *sm->current_scope, m_from_case_pattern),
        {sm->current_scope}, ERR_ARGS(*val, *val_type, *type, *type));

    // Only 1 "multi-skip" allowed in a destructure.
    raise_if<analyse::errors::SppMultipleSkipMultiArgumentsError>(
        multi_arg_skips.size() > 1,
        {sm->current_scope}, ERR_ARGS(*this, *multi_arg_skips[0], *multi_arg_skips[1]));

    // Multi skips cannot be bound.
    raise_if<analyse::errors::SppVariableObjectDestructureWithBoundMultiSkipError>(
        not multi_arg_skips.empty() and multi_arg_skips[0]->binding != nullptr,
        {sm->current_scope}, ERR_ARGS(*this, *multi_arg_skips[0]));

    // Check all attributes are provided unless there is a multi-skip.
    raise_if<analyse::errors::SppArgumentMissingError>(
        not missing_attributes.empty() and multi_arg_skips.empty(),
        {sm->current_scope}, ERR_ARGS(*missing_attributes[0], "attribute", *this, "destructure argument"));

    // Create expanded "let" statements for each part of the destructure.
    for (const auto elem : elems | genex::views::ptr) {
        // Skip any conversion for unbound multi argument skipping.
        if (elem->to<LocalVariableDestructureSkipMultipleArgumentsAst>() != nullptr) {
        }

        // Skip any conversion for single argument skipping.
        else if (const auto cast_elem_1 = elem->to<LocalVariableSingleIdentifierAst>(); cast_elem_1 != nullptr) {
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, ast_clone(cast_elem_1->name));
            auto pstfx = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(elem), nullptr, nullptr, std::move(pstfx));
            if (m_from_case_pattern) { new_ast->var->mark_from_case_pattern(); }
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }

        // Skip any conversion for single argument skipping.
        else if (elem->to<LocalVariableDestructureSkipSingleArgumentAst>() != nullptr) {
        }

        // Handle and other nested destructure or single identifier.
        else if (const auto cast_elem_2 = elem->to<LocalVariableDestructureAttributeBindingAst>(); cast_elem_2 != nullptr) {
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, ast_clone(cast_elem_2->name));
            auto pstfx = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(cast_elem_2->val), nullptr, nullptr, std::move(pstfx));
            if (m_from_case_pattern) { new_ast->var->mark_from_case_pattern(); }
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }
    }
}


auto spp::asts::LocalVariableDestructureObjectAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the elements.
    for (auto &&x : m_new_asts) {
        x->stage_8_check_memory(sm, meta);
    }
}


auto spp::asts::LocalVariableDestructureObjectAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve each element.
    for (auto &&x : m_new_asts) {
        x->stage_9_comptime_resolution(sm, meta);
    }
}


auto spp::asts::LocalVariableDestructureObjectAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the "let" statements for each element.
    for (auto &&ast : m_new_asts) {
        ast->stage_11_code_gen_2(sm, meta, ctx);
    }
    return nullptr;
}
