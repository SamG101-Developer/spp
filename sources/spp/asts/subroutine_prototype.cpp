module;
#include <spp/analyse/macros.hpp>

module spp.asts.subroutine_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.identifier_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;


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
    ast->abstract_annotation = abstract_annotation;
    ast->virtual_annotation = virtual_annotation;
    ast->temperature_annotation = temperature_annotation;
    ast->no_impl_annotation = no_impl_annotation;
    ast->inline_annotation = inline_annotation;
    ast->visibility = visibility;
    ast->llvm_func = llvm_func;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto const &a) { a->set_ast_ctx(ast); });
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
        *impl->final_member()->to<StatementAst>()->infer_type(&tm, meta), *generate::common_types_precompiled::NEVER,
        *tm.current_scope, *sm->current_scope);
    meta->restore();

    // Check there is a return statement at the end (for non-void functions).
    const auto is_void = analyse::utils::type_utils::symbolic_eq(
        *return_type, *generate::common_types_precompiled::VOID, *sm->current_scope, *sm->current_scope);

    if (is_void or is_never or no_impl_annotation or abstract_annotation or (not impl->members.empty() and impl->members.back()->to<RetStatementAst>())) {
    }
    else {
        const auto final_member = impl->final_member();
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionSubroutineMissingReturnStatementError>()
            .with_args(*final_member, *return_type)
            .raises_from(sm->current_scope);
    }

    sm->move_out_of_current_scope();
    meta->restore(true);
    meta->loop_return_types->clear();
}
