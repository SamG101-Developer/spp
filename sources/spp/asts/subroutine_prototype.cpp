#include <vector>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/ret_statement_ast.hpp>
#include <spp/asts/subroutine_prototype_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>


auto spp::asts::SubroutinePrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::stage_7_analyse_semantics(sm, meta);
    const auto ret_type_sym = sm->current_scope->get_type_symbol(*return_type);

    // Update the meta information for enclosing function information.
    meta->save();
    meta->enclosing_function_flavour = this->tok_fun.get();
    meta->enclosing_function_ret_type.emplace_back(ret_type_sym->fq_name());
    meta->enclosing_function_scope = sm->current_scope;

    // Handle the "!" never type.
    meta->save();
    meta->ignore_missing_else_branch_for_inference = true;
    const auto is_never = not impl->members.empty() and analyse::utils::type_utils::symbolic_eq(
        *impl->final_member()->infer_type(sm, meta), *generate::common_types_precompiled::NEVER,
        *sm->current_scope, *sm->current_scope);

    // Check there is a return statement at the end (for non-void functions).
    const auto is_void = analyse::utils::type_utils::symbolic_eq(
        *return_type, *generate::common_types_precompiled::VOID, *sm->current_scope, *sm->current_scope);
    if (not(is_void or is_never or m_no_impl_annotation or m_abstract_annotation or (not impl->members.empty() and ast_cast<RetStatementAst>(impl->final_member())))) {
        const auto final_member = impl->final_member();
        analyse::errors::SppFunctionSubroutineMissingReturnStatementError(*final_member, *return_type).scopes({sm->current_scope}).raise();
    }
}
