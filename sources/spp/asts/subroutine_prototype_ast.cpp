module;
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts.utils;
import genex;


spp::asts::SubroutinePrototypeAst::~SubroutinePrototypeAst() = default;


auto spp::asts::SubroutinePrototypeAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<SubroutinePrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_cmp),
        ast_clone(tok_fun),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(param_group),
        ast_clone(tok_arrow),
        ast_clone(return_type),
        ast_clone(impl));
    ast->orig_name = ast_clone(orig_name);
    ast->m_annotation_info = m_annotation_info
        ? std::make_unique<utils::AnnotationInfo>(*m_annotation_info)
        : nullptr;
    ast->m_original_impl = ast_clone(m_original_impl);
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->abstract_annotation = abstract_annotation;
    ast->virtual_annotation = virtual_annotation;
    ast->temperature_annotation = temperature_annotation;
    ast->ffi_annotation = ffi_annotation;
    ast->builtin_annotation = builtin_annotation;
    ast->inline_annotation = inline_annotation;
    ast->visibility = visibility;
    ast->m_llvm_func = m_llvm_func;
    for (auto const &a : ast->annotations) {
        a->set_ast_ctx(ast.get());
    }
    return ast;
}


auto spp::asts::SubroutinePrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::stage_7_analyse_semantics(sm, meta);
    const auto ret_type_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, return_type);

    // Update the meta information for enclosing function information.
    meta->save();
    meta->enclosing_function_flavour = this->tok_fun.get();
    meta->enclosing_function_ret_type.emplace_back(ret_type_sym->fq_name());
    meta->enclosing_function_scope = sm->current_scope;
    meta->enclosing_function_cmp = tok_cmp.get();
    impl->stage_7_analyse_semantics(sm, meta);

    // Handle the "!" never type.
    auto tm = ScopeManager(sm->global_scope, sm->current_scope->children[0].get());
    meta->save();
    meta->ignore_missing_else_branch_for_inference = true;
    const auto is_never = not impl->members.empty() and analyse::utils::type_utils::symbolic_eq(
        *impl->final_member()->to<StatementAst>()->infer_type(&tm, meta), *common_types_precompiled::NEVER,
        *tm.current_scope, *sm->current_scope);
    meta->restore();

    // Check there is a return statement at the end (for non-void functions).
    const auto is_void = analyse::utils::type_utils::symbolic_eq(
        *return_type, *common_types_precompiled::VOID,
        *sm->current_scope, *sm->current_scope);

    const auto final_member = impl->final_member();
    raise_unless<analyse::errors::SppFunctionSubroutineMissingReturnStatementError>(
        is_void or is_never or ffi_annotation or builtin_annotation or abstract_annotation or (not impl->members.empty() and impl->members.back()->to<RetStatementAst>()),
        {sm->current_scope}, ERR_ARGS(*final_member, *return_type));

    sm->move_out_of_current_scope();
    meta->restore(true);
    meta->loop_return_types->clear();
}
