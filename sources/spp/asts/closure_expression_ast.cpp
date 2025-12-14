module;
#include <spp/macros.hpp>

module spp.asts.closure_expression_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.convention_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


spp::asts::ClosureExpressionAst::ClosureExpressionAst(
    decltype(tok) &&tok,
    decltype(pc_group) &&pc_group,
    decltype(body) &&body) :
    PrimaryExpressionAst(),
    tok(std::move(tok)),
    pc_group(std::move(pc_group)),
    body(std::move(body)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok, lex::SppTokenType::KW_FUN, "fun");
}


spp::asts::ClosureExpressionAst::~ClosureExpressionAst() = default;


auto spp::asts::ClosureExpressionAst::pos_start() const
    -> std::size_t {
    return tok ? tok->pos_start() : pc_group->pos_start();
}


auto spp::asts::ClosureExpressionAst::pos_end() const
    -> std::size_t {
    return body->pos_end();
}


auto spp::asts::ClosureExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ClosureExpressionAst>(
        ast_clone(tok),
        ast_clone(pc_group),
        ast_clone(body));
}


spp::asts::ClosureExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok).append(" ");
    SPP_STRING_APPEND(pc_group).append(" ");
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok).append(" ");
    SPP_PRINT_APPEND(pc_group).append(" ");
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}


auto spp::asts::ClosureExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Save the current scope for later resetting.
    const auto parent_scope = sm->current_scope;
    pc_group->stage_7_analyse_semantics(sm, meta);

    // Update the meta args with the closure information for body analysis.
    // The closure-wide save/restore allows for the "ret" to match the closure's inferred return type.
    meta->save();
    meta->enclosing_function_scope = sm->current_scope; // this will be the closure-outer scope
    sm->current_scope->parent = sm->current_scope->parent_module();

    auto scope_name = analyse::scopes::ScopeBlockName("<closure-inner#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    meta->enclosing_function_flavour = tok.get();
    meta->enclosing_function_ret_type = {};

    // Analyse the body of the closure.
    body->stage_7_analyse_semantics(sm, meta);
    m_ret_type = not meta->enclosing_function_ret_type.empty() ? meta->enclosing_function_ret_type[0] : body->infer_type(sm, meta);
    meta->restore(true);

    // Set the scope back.
    sm->current_scope = parent_scope;
}


auto spp::asts::ClosureExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Save the current scope for later resetting.
    const auto parent_scope = sm->current_scope;
    meta->save();
    pc_group->stage_8_check_memory(sm, meta);

    // Check the memory of the body of the closure.
    sm->move_to_next_scope();
    body->stage_8_check_memory(sm, meta);

    // Set the scope back.
    meta->restore();
    sm->current_scope = parent_scope;
}


auto spp::asts::ClosureExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // TODO
    // For now, just skip scopes and return a nullptr.
    const auto parent_scope = sm->current_scope;
    pc_group->stage_10_code_gen_2(sm, meta, ctx);
    sm->move_to_next_scope();
    body->stage_10_code_gen_2(sm, meta, ctx);
    sm->current_scope = parent_scope;
    return nullptr;
}


auto spp::asts::ClosureExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Create the type as a nullptr, so it can be analysed later.
    std::shared_ptr<TypeAst> ty = nullptr;

    auto is_ref_cap = [](auto const &cap) { return cap->conv and *cap->conv == ConventionTag::REF; };
    auto is_mut_cap = [](auto const &cap) { return cap->conv and *cap->conv == ConventionTag::MUT; };

    // If there are no captures, return a FunRef type with the parameters and return type.
    if (pc_group->capture_group->captures.empty()) {
        auto param_types = pc_group->param_group->params
            | genex::views::transform([](auto const &x) { return x->type; })
            | genex::to<std::vector>();
        ty = generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    else if (genex::any_of(pc_group->capture_group->captures, [](auto const &x) { return x->conv == nullptr; })) {
        // If there are captures, but no borrowed captures, return a FunMov type with the parameters and return type.
        auto param_types = pc_group->param_group->params
            | genex::views::transform([](auto const &x) { return x->type; })
            | genex::to<std::vector>();
        ty = generate::common_types::fun_mov_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    else if (genex::any_of(pc_group->capture_group->captures, is_mut_cap)) {
        // If there are mutably borrowed captures, return a FunMut type with the parameters and return type.
        auto param_types = pc_group->param_group->params
            | genex::views::transform([](auto const &x) { return x->type; })
            | genex::to<std::vector>();
        ty = generate::common_types::fun_mut_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    else if (genex::any_of(pc_group->capture_group->captures, is_ref_cap)) {
        // If there are immutable borrowed captures, return a FunRef type with the parameters and return type.
        auto param_types = pc_group->param_group->params
            | genex::views::transform([](auto const &x) { return x->type; })
            | genex::to<std::vector>();
        ty = generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    // Analyse the type and return it.
    ty->stage_7_analyse_semantics(sm, meta);
    return ty;
}
