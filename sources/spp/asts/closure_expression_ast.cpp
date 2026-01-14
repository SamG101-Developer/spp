module;
#include <spp/macros.hpp>

module spp.asts.closure_expression_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
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
    auto c = std::make_unique<ClosureExpressionAst>(
        ast_clone(tok),
        ast_clone(pc_group),
        ast_clone(body));
    c->m_ret_type = m_ret_type;
    return c;
}


spp::asts::ClosureExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok).append(" ");
    SPP_STRING_APPEND(pc_group).append(" ");
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Save the current scope for later resetting.
    const auto parent_scope = sm->current_scope;
    pc_group->stage_7_analyse_semantics(sm, meta);

    const auto inherited_type_generics = sm->current_scope->all_type_symbols()
        | genex::views::filter([](auto const &sym) { return sym->is_generic; })
        | genex::to<std::vector>();

    const auto inherited_comp_generics = sm->current_scope->all_var_symbols()
        | genex::views::filter([](auto const &sym) { return sym->is_generic; })
        | genex::to<std::vector>();

    // Update the meta args with the closure information for body analysis.
    // The closure-wide save/restore allows for the "ret" to match the closure's inferred return type.
    meta->save();
    meta->enclosing_function_scope = sm->current_scope; // this will be the closure-outer scope
    sm->current_scope->parent = sm->current_scope->parent_module();

    auto scope_name = analyse::scopes::ScopeBlockName("<closure-inner#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    meta->enclosing_function_flavour = tok.get();
    meta->enclosing_function_ret_type = {};

    // Add the inherited generics into the closure-inner scope.
    for (const auto &type_generic_sym : inherited_type_generics) {
        sm->current_scope->add_type_symbol(type_generic_sym);
    }
    for (const auto &comp_generic_sym : inherited_comp_generics) {
        sm->current_scope->add_var_symbol(comp_generic_sym);
    }

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
    // Strategy: build an "environment" struct for the closure. Attributes are captures. The safety is already
    // guaranteed by semantic analysis.
    // Todo: Add LLVM attributes to pointer types for optimizations (unique, nonnull, etc).
    const auto uid = spp::utils::generate_uid(this);
    const auto env_ty = llvm::StructType::create(*ctx->context, "$ClosureEnv" + uid);
    auto env_field_types = std::vector<llvm::Type*>{};
    for (const auto &capture : pc_group->capture_group->captures) {
        const auto cap_ty = capture->infer_type(sm, meta);
        const auto cap_ty_sym = sm->current_scope->get_type_symbol(cap_ty);
        env_field_types.emplace_back(codegen::llvm_type(*cap_ty_sym, ctx));
    }
    env_ty->setBody(env_field_types, false);

    // Build a new function that the body of the closure is built into. Needs a variable binding at the top (ie allow
    // "let a = env.a" as the function signature will be "(env: $ClosureX, ...params: Params) -> RetType").
    auto llvm_param_types = pc_group->param_group->get_all_params()
        | genex::views::transform([&](auto const &param) { return codegen::llvm_type(*sm->current_scope->get_type_symbol(param->type), ctx); })
        | genex::to<std::vector>();
    llvm_param_types.insert(llvm_param_types.begin(), llvm::PointerType::get(*ctx->context, 0));
    const auto llvm_ret_ty = codegen::llvm_type(*sm->current_scope->get_type_symbol(m_ret_type), ctx);
    const auto llvm_fn_ty = llvm::FunctionType::get(llvm_ret_ty, llvm_param_types, pc_group->param_group->get_variadic_param() != nullptr);
    const auto llvm_fn = llvm::Function::Create(llvm_fn_ty, llvm::Function::InternalLinkage, "$closure_fn_" + uid, ctx->module.get());
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->context, "entry", llvm_fn);

    const auto saved_bb = ctx->builder.GetInsertBlock();
    const auto saved_fn_scope = meta->enclosing_function_scope;
    const auto saved_ret_ty = meta->enclosing_function_ret_type;
    const auto saved_flavour = meta->enclosing_function_flavour;
    const auto saved_current_closure_type = ctx->current_closure_type;

    ctx->builder.SetInsertPoint(entry_bb);
    sm->current_scope->ast = this;
    llvm_func = llvm_fn;
    meta->enclosing_function_scope = sm->current_scope;
    meta->enclosing_function_ret_type = {m_ret_type};
    meta->enclosing_function_flavour = tok.get();
    ctx->current_closure_type = env_ty;
    ctx->current_closure_scope = sm->current_scope;

    // For now, just skip scopes and return a nullptr.
    const auto parent_scope = sm->current_scope;
    pc_group->stage_10_code_gen_2(sm, meta, ctx);
    sm->move_to_next_scope();
    body->stage_10_code_gen_2(sm, meta, ctx);
    sm->current_scope = parent_scope;

    // Restore the previous context.
    ctx->builder.SetInsertPoint(saved_bb);
    meta->enclosing_function_scope = saved_fn_scope;
    meta->enclosing_function_ret_type = saved_ret_ty;
    meta->enclosing_function_flavour = saved_flavour;
    ctx->current_closure_type = saved_current_closure_type;

    // Allocate the closure environment.
    const auto env_alloca = ctx->builder.CreateAlloca(env_ty, nullptr, "closure.env.alloca." + uid);
    for (const auto &[i, capture] : pc_group->capture_group->captures | genex::views::ptr | genex::views::enumerate) {
        const auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), 0);
        const auto capture_index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), i);

        const auto field_ptr = ctx->builder.CreateInBoundsGEP(
            env_ty, env_alloca, {zero, capture_index}, "closure.env.gep." + std::to_string(i));
        const auto val = capture->val->stage_10_code_gen_2(sm, meta, ctx); // always an identifier, no worry abt double generation.
        ctx->builder.CreateStore(val, field_ptr);
    }

    // Allocate the closure object.
    const auto closure_ty = llvm::StructType::create(*ctx->context, "$ClosureObj" + uid);
    const auto closure_alloca = ctx->builder.CreateAlloca(closure_ty, nullptr, "closure.obj.alloca." + uid);
    ctx->builder.CreateStore(
        llvm_fn,
        ctx->builder.CreateInBoundsGEP(closure_ty, closure_alloca, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), 0)}));

    ctx->builder.CreateStore(
        env_alloca,
        ctx->builder.CreateInBoundsGEP(closure_ty, closure_alloca, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), 1)}));

    // Return the generated closure.
    return ctx->builder.CreateLoad(closure_ty, closure_alloca, "load.closure.obj." + uid);
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
