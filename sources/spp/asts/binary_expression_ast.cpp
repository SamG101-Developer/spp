module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.binary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.bin_utils;
import spp.analyse.utils.type_utils;
import spp.asts.fold_expression_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::BinaryExpressionAst::BinaryExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    m_mapped_func(nullptr),
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::BinaryExpressionAst::~BinaryExpressionAst() = default;


auto spp::asts::BinaryExpressionAst::pos_start() const
    -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::BinaryExpressionAst::pos_end() const
    -> std::size_t {
    return rhs ? rhs->pos_end() : m_mapped_func->pos_end();
}


auto spp::asts::BinaryExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<BinaryExpressionAst>(
        ast_clone(lhs),
        ast_clone(tok_op),
        ast_clone(rhs));
    ast->m_mapped_func = m_mapped_func;
    return ast;
}


spp::asts::BinaryExpressionAst::operator std::string() const {
    SPP_STRING_START;
    if (lhs != nullptr) {
        raw_string.append("(");
        SPP_STRING_APPEND(lhs).append(" ");
        SPP_STRING_APPEND(tok_op).append(" ");
        SPP_STRING_APPEND(rhs).append(")");
    }
    else {
        SPP_STRING_APPEND(m_mapped_func);
    }
    SPP_STRING_END;
}


auto spp::asts::BinaryExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    if (lhs != nullptr) {
        formatted_string.append("(");
        SPP_PRINT_APPEND(lhs).append(" ");
        SPP_PRINT_APPEND(tok_op).append(" ");
        SPP_PRINT_APPEND(rhs).append(")");
    }
    else {
        SPP_PRINT_APPEND(m_mapped_func);
    }
    SPP_PRINT_END;
}


auto spp::asts::BinaryExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Ensure TypeAst's aren't used for expression for binary operands.
    SPP_ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TOKEN(lhs.get());
    SPP_ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TOKEN(rhs.get());

    // Check compound assignment (for example "+=") has a symbolic lhs target.
    if (genex::contains(analyse::utils::bin_utils::BIN_COMPOUND_ASSIGNMENT_OPS, tok_op->token_type)) {
        if (not sm->current_scope->get_var_symbol_outermost(*lhs).first) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppAssignmentTargetError>()
                .with_args(*lhs)
                .raises_from(sm->current_scope);
        }
    }

    // Handle lhs-folding.
    if (lhs->to<FoldExpressionAst>()) {
        // Check the rhs is a tuple.
        const auto rhs_tuple_type = rhs->infer_type(sm, meta);
        if (not analyse::utils::type_utils::is_type_tuple(*rhs_tuple_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessNonIndexableError>()
                .with_args(*rhs, *rhs_tuple_type, *lhs)
                .raises_from(sm->current_scope);
        }

        // Get the parts of the tuple.
        const auto rhs_num_elems = rhs_tuple_type->type_parts()[0]->generic_arg_group->args.size();
        auto new_asts = std::vector<std::unique_ptr<PostfixExpressionAst>>();
        for (auto i = 0u; i < rhs_num_elems; ++i) {
            auto field = std::make_unique<IdentifierAst>(rhs->pos_start(), std::to_string(i));
            auto new_ast = std::make_unique<PostfixExpressionAst>(
                ast_clone(rhs),
                std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            new_ast->stage_7_analyse_semantics(sm, meta);
            new_asts.emplace_back(std::move(new_ast));
        }

        // Convert "t = (0, 1, 2, 3)", ".. + t" into "(((t.0 + t.1) + t.2) + t.3)".
        lhs = std::move(new_asts[0]);
        rhs = std::move(new_asts[1]);
        for (auto &&new_ast : new_asts | genex::views::move | genex::views::drop(2)) {
            lhs = std::make_unique<BinaryExpressionAst>(std::move(lhs), ast_clone(tok_op), std::move(rhs));
            rhs = std::move(new_ast);
        }
        m_mapped_func = analyse::utils::bin_utils::convert_bin_expr_to_function_call(*this, sm, meta);
        m_mapped_func->stage_7_analyse_semantics(sm, meta);
    }

    // Handle rhs-folding.
    else if (rhs->to<FoldExpressionAst>()) {
        // Check the lhs is a tuple.
        const auto lhs_tuple_type = lhs->infer_type(sm, meta);
        if (not analyse::utils::type_utils::is_type_tuple(*lhs_tuple_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessNonIndexableError>()
                .with_args(*lhs, *lhs_tuple_type, *rhs)
                .raises_from(sm->current_scope);
        }

        // Get the parts of the tuple.
        const auto lhs_num_elems = lhs_tuple_type->type_parts()[0]->generic_arg_group->args.size();
        auto new_asts = std::vector<std::unique_ptr<PostfixExpressionAst>>();
        for (auto i = 0u; i < lhs_num_elems; ++i) {
            auto field = std::make_unique<IdentifierAst>(lhs->pos_start(), std::to_string(i));
            auto new_ast = std::make_unique<PostfixExpressionAst>(
                ast_clone(lhs),
                std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            new_ast->stage_7_analyse_semantics(sm, meta);
            new_asts.emplace_back(std::move(new_ast));
        }

        // Convert "t = (0, 1, 2, 3)", "t + .." into "(t.0 + (t.1 + (t.2 + t.3)))".
        lhs = std::move(new_asts[new_asts.size() - 2]);
        rhs = std::move(new_asts[new_asts.size() - 1]);
        for (auto &&new_ast : new_asts | genex::views::move_reverse | genex::views::drop(2)) {
            rhs = std::make_unique<BinaryExpressionAst>(std::move(lhs), ast_clone(tok_op), std::move(rhs));
            lhs = std::move(new_ast);
        }
        m_mapped_func = analyse::utils::bin_utils::convert_bin_expr_to_function_call(*this, sm, meta);
        m_mapped_func->stage_7_analyse_semantics(sm, meta);
    }

    else {
        // Standard non-folding binary expression.
        m_mapped_func = analyse::utils::bin_utils::convert_bin_expr_to_function_call(*this, sm, meta);
        m_mapped_func->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::BinaryExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the memory checking to the mapped function.
    m_mapped_func->stage_8_check_memory(sm, meta);
}


auto spp::asts::BinaryExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Forward the code generation to the mapped function.
    return m_mapped_func->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::BinaryExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type from the function mapping of the binary expression.
    if (m_mapped_func == nullptr) {
        stage_7_analyse_semantics(sm, meta);
    }
    return m_mapped_func->infer_type(sm, meta);
}
