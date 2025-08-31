#include <format>

#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/closure_expression_ast.hpp>
#include <spp/asts/closure_expression_capture_ast.hpp>
#include <spp/asts/closure_expression_capture_group_ast.hpp>
#include <spp/asts/closure_expression_parameter_and_capture_group_ast.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/algorithms/any_of.hpp>
#include <genex/views/to.hpp>


spp::asts::ClosureExpressionAst::ClosureExpressionAst(
    decltype(tok) &&tok,
    decltype(pc_group) &&pc_group,
    decltype(body) &&body) :
    PrimaryExpressionAst(),
    tok(std::move(tok)),
    pc_group(std::move(pc_group)),
    body(std::move(body)) {
    if (this->tok == nullptr) {
        this->tok = std::make_unique<TokenAst>(pos_start(), lex::SppTokenType::KW_FUN, "fun");
    }
}


spp::asts::ClosureExpressionAst::~ClosureExpressionAst() = default;


auto spp::asts::ClosureExpressionAst::pos_start() const -> std::size_t {
    return tok ? tok->pos_start() : pc_group->pos_start();
}


auto spp::asts::ClosureExpressionAst::pos_end() const -> std::size_t {
    return body->pos_end();
}


auto spp::asts::ClosureExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ClosureExpressionAst>(
        ast_clone(tok),
        ast_clone(pc_group),
        ast_clone(body));
}


spp::asts::ClosureExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok);
    SPP_STRING_APPEND(pc_group);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok);
    SPP_PRINT_APPEND(pc_group);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}


auto spp::asts::ClosureExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Save the current scope for later resetting.
    const auto parent_scope = sm->current_scope;
    pc_group->stage_7_analyse_semantics(sm, meta);

    // Update the meta args with the closure information for body analysis.
    meta->save();
    meta->enclosing_function_scope = sm->current_scope;
    auto scope_name = analyse::scopes::ScopeBlockName("<lambda-inner#" + std::to_string(pos_start()));
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    meta->enclosing_function_flavour = tok.get();
    meta->enclosing_function_ret_type = {};
    meta->current_lambda_outer_scope = parent_scope;

    // Analyse the body of the closure.
    body->stage_7_analyse_semantics(sm, meta);
    const auto body_type = body->infer_type(sm, meta);
    m_ret_type = std::move(not meta->enclosing_function_ret_type.empty() ? meta->enclosing_function_ret_type[0] : body_type);
    meta->restore();

    // Set the scope back.
    sm->current_scope = parent_scope;
}


auto spp::asts::ClosureExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Save the current scope for later resetting.
    const auto parent_scope = sm->current_scope;
    meta->save();
    meta->current_lambda_outer_scope = parent_scope;
    pc_group->stage_8_check_memory(sm, meta);

    // Check the memory of the body of the closure.
    sm->move_to_next_scope();
    body->stage_8_check_memory(sm, meta);
    meta->restore();

    // Set the scope back.
    sm->current_scope = parent_scope;
    pc_group->stage_8_check_memory(sm, meta);
}


auto spp::asts::ClosureExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Create the type as a nullptr, so it can be analysed later.
    std::unique_ptr<TypeAst> ty = nullptr;

    // If there are no captures, return a FunRef type with the parameters and return type.
    if (pc_group->capture_group->captures.empty()) {
        auto param_types = pc_group->param_group->params | genex::views::map([](auto &&x) { return x->type; }) | genex::views::to<std::vector>();
        ty = generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    else if (genex::algorithms::any_of(pc_group->capture_group->captures, [](auto &&x) { return x->conv->tag == ConventionAst::ConventionTag::MOV; })) {
        // If there are captures, but no borrowed captures, return a FunMov type with the parameters and return type.
        auto param_types = pc_group->param_group->params | genex::views::map([](auto &&x) { return x->type; }) | genex::views::to<std::vector>();
        ty = generate::common_types::fun_mov_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    else if (genex::algorithms::any_of(pc_group->capture_group->captures, [](auto &&x) { return x->conv->tag == ConventionAst::ConventionTag::MUT; })) {
        // If there are mutably borrowed captures, return a FunMut type with the parameters and return type.
        auto param_types = pc_group->param_group->params | genex::views::map([](auto &&x) { return x->type; }) | genex::views::to<std::vector>();
        ty = generate::common_types::fun_mut_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    else if (genex::algorithms::any_of(pc_group->capture_group->captures, [](auto &&x) { return x->conv->tag == ConventionAst::ConventionTag::REF; })) {
        // If there are immutable borrowed captures, return a FunRef type with the parameters and return type.
        auto param_types = pc_group->param_group->params | genex::views::map([](auto &&x) { return x->type; }) | genex::views::to<std::vector>();
        ty = generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), m_ret_type);
    }

    // Analyse the type and return it.
    ty->stage_7_analyse_semantics(sm, meta);
    return ty;
}
