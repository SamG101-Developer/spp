module;
#include <spp/macros.hpp>

module spp.asts.closure_expression_capture_group_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.closure_expression_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import genex;


spp::asts::ClosureExpressionCaptureGroupAst::ClosureExpressionCaptureGroupAst(
    decltype(tok_caps) &&tok_caps,
    decltype(captures) &&captures) :
    tok_caps(std::move(tok_caps)),
    captures(std::move(captures)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_caps, lex::SppTokenType::KW_CAPS, "caps");
}


spp::asts::ClosureExpressionCaptureGroupAst::~ClosureExpressionCaptureGroupAst() = default;


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_start() const
    -> std::size_t {
    return tok_caps->pos_start();
}


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_end() const
    -> std::size_t {
    return captures.back()->pos_end();
}


auto spp::asts::ClosureExpressionCaptureGroupAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ClosureExpressionCaptureGroupAst>(
        ast_clone(tok_caps),
        ast_clone_vec(captures));
}


spp::asts::ClosureExpressionCaptureGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_caps).append(" ");
    SPP_STRING_EXTEND(captures, ", ");
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionCaptureGroupAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_caps).append(" ");
    SPP_PRINT_EXTEND(captures, ", ");
    SPP_PRINT_END;
}


auto spp::asts::ClosureExpressionCaptureGroupAst::new_empty()
    -> std::unique_ptr<ClosureExpressionCaptureGroupAst> {
    return std::make_unique<ClosureExpressionCaptureGroupAst>(
        nullptr, decltype(captures)());
}


auto spp::asts::ClosureExpressionCaptureGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Add the capture variables after analysis, otherwise their symbol checks refer to the new captures, not the
    // original asts from the argument group analysis.
    for (auto &&cap : captures) {
        // Create a "let" statement to insert the symbol into the current scope.
        auto cap_val = ast_clone(cap->val->to<IdentifierAst>());
        auto var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(cap_val), nullptr);
        auto var_type = cap->val->infer_type(sm, meta);
        auto let_val = std::make_unique<ObjectInitializerAst>(std::move(var_type), nullptr);
        const auto let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, std::move(let_val));
        let->stage_7_analyse_semantics(sm, meta);

        // Apply the borrow to the symbol.
        const auto sym = sm->current_scope->get_var_symbol(ast_clone(cap->val->to<IdentifierAst>()));
        const auto conv = cap->conv.get();
        sym->memory_info->ast_borrowed = {conv, sm->current_scope};
        sym->type = sym->type->with_convention(ast_clone(cap->conv));
    }
}


auto spp::asts::ClosureExpressionCaptureGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Any borrowed captures need pinning and marking as extended borrows.
    const auto [ass_sym, _] = meta->current_lambda_outer_scope->get_var_symbol_outermost(*meta->assignment_target);
    for (auto const &cap : captures) {
        if (cap->conv != nullptr) {
            // Mark the pins on the capture and the target.
            const auto cap_val = cap->val->to<IdentifierAst>();
            const auto cap_sym = sm->current_scope->get_var_symbol(ast_clone(cap_val));
            if (ass_sym != nullptr) { ass_sym->memory_info->ast_pins.emplace_back(cap->val.get()); }
            if (cap_sym != nullptr) { cap_sym->memory_info->ast_pins.emplace_back(cap->val.get()); }

            // Mark the extended borrow. TODO
            // auto is_mut = *cap->conv == ConventionAst::ConventionTag::MUT;
            // cap_sym->memory_info->extended_borrows.emplace_back(
            //     cap->val.get(), is_mut, meta->current_lambda_outer_scope);
        }
    }
}


auto spp::asts::ClosureExpressionCaptureGroupAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Build the variable bindings from the environment object. This allows the body to remain unchanged as the
    // variables get loaded from the environment struct.
    for (auto const &[i, capture] : captures | genex::views::ptr | genex::views::enumerate) {
        const auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), 0);
        const auto idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), i);

        // For the capture x, mock "let x = env.x".
        const auto cap_val = capture->val->to<IdentifierAst>();
        const auto cap_ty = capture->infer_type(sm, meta);
        const auto cap_ty_sym = sm->current_scope->get_type_symbol(cap_ty);
        const auto cap_llvm_type = codegen::llvm_type(*sm->current_scope->get_type_symbol(cap_ty), ctx);

        // Create the alloca for the variable.
        const auto alloca = ctx->builder.CreateAlloca(
            cap_llvm_type, nullptr, "capture.alloca." + cap_val->val);

        const auto gep = ctx->builder.CreateInBoundsGEP(
            ctx->current_closure_type,
            ctx->current_closure_scope->ast->to<ClosureExpressionAst>()->llvm_func->getArg(0),
            std::vector<llvm::Value*>{zero, idx});

        const auto load = ctx->builder.CreateLoad(cap_llvm_type, gep, "capture.load." + cap_val->val);

        ctx->builder.CreateStore(load, alloca);

        // Add the alloca to the current scope as a variable symbol.
        // Todo: Handle mutability properly.
        auto var_sym = std::make_unique<analyse::scopes::VariableSymbol>(
            asts::ast_clone(cap_val), cap_ty, false, false);
        var_sym->llvm_info->alloca = alloca;
        sm->current_scope->add_var_symbol(std::move(var_sym));
    }
    return nullptr;
}
