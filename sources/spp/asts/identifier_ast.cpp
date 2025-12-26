module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.identifier_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.utils.strings;
import spp.asts.utils.ast_utils;
import absl;
import genex;
import llvm;


spp::asts::IdentifierAst::IdentifierAst(
    const std::size_t pos,
    decltype(val) val) :
    std::enable_shared_from_this<IdentifierAst>(),
    m_pos(pos),
    val(std::move(val)) {
}


auto spp::asts::IdentifierAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_identifier(*this);
}


auto spp::asts::IdentifierAst::equals_identifier(
    IdentifierAst const &other) const
    -> std::strong_ordering {
    if (val == other.val) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::IdentifierAst::pos_start() const
    -> std::size_t {
    return m_pos;
}


auto spp::asts::IdentifierAst::pos_end() const
    -> std::size_t {
    return m_pos + val.length();
}


auto spp::asts::IdentifierAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IdentifierAst>(m_pos, std::string(val));
}


spp::asts::IdentifierAst::operator std::string() const {
    return val;
}


auto spp::asts::IdentifierAst::print(
    AstPrinter &) const
    -> std::string {
    return val;
}


auto spp::asts::IdentifierAst::operator+(
    IdentifierAst const &that) const
    -> IdentifierAst {
    return IdentifierAst(m_pos, val + that.val);
}


auto spp::asts::IdentifierAst::operator+(
    std::string const &that) const
    -> IdentifierAst {
    return IdentifierAst(m_pos, val + that);
}


auto spp::asts::IdentifierAst::from_type(
    TypeAst const &val)
    -> std::unique_ptr<IdentifierAst> {
    return std::make_unique<IdentifierAst>(val.pos_start(), std::string(val.type_parts().back()->name));
}


auto spp::asts::IdentifierAst::to_function_identifier() const
    -> std::unique_ptr<IdentifierAst> {
    return std::make_unique<IdentifierAst>(m_pos, "$" + spp::utils::strings::snake_to_pascal(val));
}


auto spp::asts::IdentifierAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Check there is a symbol with the same name in the current scope.
    const auto shared = std::shared_ptr(ast_clone(this));
    if (not sm->current_scope->has_var_symbol(shared) and not sm->current_scope->has_ns_symbol(shared)) {
        const auto alternatives = sm->current_scope->all_var_symbols()
            | genex::views::transform([](auto &&x) { return x->name->val; })
            | genex::to<std::vector>();

        const auto closest_match = spp::utils::strings::closest_match(val, alternatives);
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierUnknownError>()
            .with_args(*this, "identifier", closest_match)
            .raises_from(sm->current_scope);
    }
}


auto spp::asts::IdentifierAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the allocation for the variable from the current scope.
    const auto var_sym = sm->current_scope->get_var_symbol(ast_clone(this));
    SPP_ASSERT(var_sym->llvm_info->alloca != nullptr);

    // Handle local variable allocation extraction + load.
    if (llvm::isa<llvm::AllocaInst>(var_sym->llvm_info->alloca)) {
        const auto alloca = llvm::cast<llvm::AllocaInst>(var_sym->llvm_info->alloca);
        return ctx->builder.CreateLoad(alloca->getAllocatedType(), alloca, "load.local");
    }

    // Handle global variable (load from global).
    if (llvm::isa<llvm::GlobalVariable>(var_sym->llvm_info->alloca)) {
        const auto global_var = llvm::cast<llvm::GlobalVariable>(var_sym->llvm_info->alloca);
        return ctx->builder.CreateLoad(global_var->getValueType(), global_var, "load.global");
    }

    std::unreachable();
}


auto spp::asts::IdentifierAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Extract the symbol from the current scope, as a variable symbol.
    const auto var_sym = sm->current_scope->get_var_symbol(ast_clone(this));
    return var_sym ? var_sym->type : nullptr;
}


auto spp::asts::IdentifierAst::ankerl_hash() const
    -> std::size_t {
    return absl::Hash<std::string>()(val);
}
