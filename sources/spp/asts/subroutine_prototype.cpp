module;
#include <genex/views/for_each.hpp>

module spp.asts.subroutine_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.function_implementation_ast;
import spp.asts.statement_ast;
import spp.asts.generate.common_types_precompiled;


spp::asts::SubroutinePrototypeAst::~SubroutinePrototypeAst() = default;


auto spp::asts::SubroutinePrototypeAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<SubroutinePrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_fun),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(param_group),
        ast_clone(tok_arrow),
        ast_clone(return_type),
        ast_clone(impl));
    ast->orig_name = ast_clone(orig_name);
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->m_abstract_annotation = m_abstract_annotation;
    ast->m_virtual_annotation = m_virtual_annotation;
    ast->m_temperature_annotation = m_temperature_annotation;
    ast->m_no_impl_annotation = m_no_impl_annotation;
    ast->m_inline_annotation = m_inline_annotation;
    ast->m_visibility = m_visibility;
    ast->m_llvm_func = m_llvm_func;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto const &a) { a->m_ctx = ast; });
    return ast;
}


auto spp::asts::SubroutinePrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::stage_7_analyse_semantics(sm, meta);
    const auto ret_type_sym = sm->current_scope->get_type_symbol(return_type);

    // Update the meta information for enclosing function information.
    meta->save();
    meta->enclosing_function_flavour = this->tok_fun.get();
    meta->enclosing_function_ret_type.emplace_back(ret_type_sym->fq_name());
    meta->enclosing_function_scope = sm->current_scope;
    impl->stage_7_analyse_semantics(sm, meta);

    // Handle the "!" never type.
    auto tm = ScopeManager(sm->global_scope, sm->current_scope->children[0].get());
    meta->save();
    meta->ignore_missing_else_branch_for_inference = true;
    const auto is_never = not impl->members.empty() and analyse::utils::type_utils::symbolic_eq(
        *ast_cast<StatementAst>(impl->final_member())->infer_type(&tm, meta), *generate::common_types_precompiled::NEVER,
        *tm.current_scope, *sm->current_scope);
    meta->restore();

    // Check there is a return statement at the end (for non-void functions).
    const auto is_void = analyse::utils::type_utils::symbolic_eq(
        *return_type, *generate::common_types_precompiled::VOID, *sm->current_scope, *sm->current_scope);

    if (is_void or is_never or m_no_impl_annotation or m_abstract_annotation or (not impl->members.empty() and ast_cast<RetStatementAst>(impl->members.back().get()))) {
    }
    else {
        const auto final_member = impl->final_member();
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionSubroutineMissingReturnStatementError>().with_args(
            *final_member, *return_type).with_scopes({sm->current_scope}).raise();
    }

    sm->move_out_of_current_scope();
    meta->restore(true);
    meta->loop_return_types->clear();
}
