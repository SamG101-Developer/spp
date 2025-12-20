module spp.asts.meta.compiler_meta_data;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import ankerl;


spp::asts::meta::CompilerMetaData::CompilerMetaData() {
    current_stage = 0;
    return_type_overload_resolver_type = nullptr;
    assignment_target = nullptr;
    assignment_target_type = nullptr;
    ignore_missing_else_branch_for_inference = false;
    case_condition = nullptr;
    cls_sym = nullptr;
    enclosing_function_scope = nullptr;
    enclosing_function_flavour = nullptr;
    enclosing_function_ret_type = {};
    current_lambda_outer_scope = nullptr;
    target_call_function_prototype = nullptr;
    target_call_was_function_async = false;
    prevent_auto_generator_resume = false;
    let_stmt_explicit_type = nullptr;
    let_stmt_value = nullptr;
    let_stmt_from_uninitialized = false;
    loop_double_check_active = false;
    current_loop_depth = 0;
    current_loop_ast = nullptr;
    loop_return_types = std::make_unique<std::map<std::size_t, std::tuple<ExpressionAst*, std::shared_ptr<TypeAst>, analyse::scopes::Scope*>>>();
    object_init_type = nullptr;
    infer_source = {};
    infer_target = {};
    postfix_expression_lhs = nullptr;
    unary_expression_rhs = nullptr;
    skip_type_analysis_generic_checks = false;
    type_analysis_type_scope = nullptr;
    ignore_cmp_generic = nullptr;
    phi_node = nullptr;
    end_bb = nullptr;
    llvm_ctx = nullptr;
}


auto spp::asts::meta::CompilerMetaData::save() -> void {
    m_history.emplace(
        current_stage, return_type_overload_resolver_type, assignment_target,
        assignment_target_type, ignore_missing_else_branch_for_inference, case_condition, cls_sym,
        enclosing_function_scope, enclosing_function_flavour, enclosing_function_ret_type,
        current_lambda_outer_scope, target_call_function_prototype, target_call_was_function_async,
        prevent_auto_generator_resume, let_stmt_explicit_type, let_stmt_value, let_stmt_from_uninitialized,
        loop_double_check_active, current_loop_depth, current_loop_ast, loop_return_types, object_init_type,
        infer_source, infer_target, postfix_expression_lhs, unary_expression_rhs, skip_type_analysis_generic_checks,
        type_analysis_type_scope, ignore_cmp_generic, phi_node, end_bb, llvm_ctx);
}


auto spp::asts::meta::CompilerMetaData::restore(const bool heavy) -> void {
    auto state = std::move(m_history.top()); // *DO NOT* click "convert to structured bindings" (CLion) -- LAG
    m_history.pop();
    current_stage = state.current_stage;
    return_type_overload_resolver_type = state.return_type_overload_resolver_type;
    assignment_target = state.assignment_target;
    assignment_target_type = state.assignment_target_type;
    ignore_missing_else_branch_for_inference = state.ignore_missing_else_branch_for_inference;
    case_condition = state.case_condition;
    cls_sym = state.cls_sym;
    if (heavy) {
        enclosing_function_scope = state.enclosing_function_scope;
        enclosing_function_flavour = state.enclosing_function_flavour;
        enclosing_function_ret_type = state.enclosing_function_ret_type;
    }
    current_lambda_outer_scope = state.current_lambda_outer_scope;
    target_call_function_prototype = state.target_call_function_prototype;
    target_call_was_function_async = state.target_call_was_function_async;
    prevent_auto_generator_resume = state.prevent_auto_generator_resume;
    let_stmt_explicit_type = state.let_stmt_explicit_type;
    let_stmt_value = state.let_stmt_value;
    let_stmt_from_uninitialized = state.let_stmt_from_uninitialized;
    loop_double_check_active = state.loop_double_check_active;
    current_loop_depth = state.current_loop_depth;
    current_loop_ast = state.current_loop_ast;
    loop_return_types = state.loop_return_types;
    object_init_type = state.object_init_type;
    infer_source = state.infer_source;
    infer_target = state.infer_target;
    postfix_expression_lhs = state.postfix_expression_lhs;
    unary_expression_rhs = state.unary_expression_rhs;
    skip_type_analysis_generic_checks = state.skip_type_analysis_generic_checks;
    type_analysis_type_scope = state.type_analysis_type_scope;
    ignore_cmp_generic = state.ignore_cmp_generic;
    phi_node = state.phi_node;
    end_bb = state.end_bb;
    llvm_ctx = state.llvm_ctx;
}


auto spp::asts::meta::CompilerMetaData::depth() const
    -> std::size_t {
    return m_history.size();
}
