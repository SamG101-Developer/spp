module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_comp_keyword_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.mem_utils;
import spp.asts.token_ast;
import spp.asts.ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.lex.tokens;


spp::asts::GenericArgumentCompKeywordAst::GenericArgumentCompKeywordAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    GenericArgumentCompAst(std::move(val), mixins::OrderableTag::KEYWORD_ARG),
    name(std::move(name)),
    tok_assign(std::move(tok_assign)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


spp::asts::GenericArgumentCompKeywordAst::~GenericArgumentCompKeywordAst() = default;


auto spp::asts::GenericArgumentCompKeywordAst::equals(
    GenericArgumentAst const &other) const
    -> std::strong_ordering {
    return other.equals_generic_argument_comp_keyword(*this);
}


auto spp::asts::GenericArgumentCompKeywordAst::equals_generic_argument_comp_keyword(
    GenericArgumentCompKeywordAst const &other) const
    -> std::strong_ordering {
    if (*name == *other.name and *val == *other.val) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentCompKeywordAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericArgumentCompKeywordAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::GenericArgumentCompKeywordAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericArgumentCompKeywordAst>(
        ast_clone(name),
        ast_clone(tok_assign),
        ast_clone(val));
}


spp::asts::GenericArgumentCompKeywordAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentCompKeywordAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::GenericArgumentCompKeywordAst::from_symbol(
    analyse::scopes::VariableSymbol const &sym)
    -> std::unique_ptr<GenericArgumentCompKeywordAst> {
    // Get the comptime value from the symbol's memory info.
    const auto c = sym.memory_info->ast_comptime.get();
    std::unique_ptr<ExpressionAst> value = nullptr;

    // Depending on that the comptime AST is, get the value.
    if (const auto comptime_param = ast_cast<GenericParameterCompAst>(c)) {
        value = asts::ast_cast<ExpressionAst>(ast_clone(comptime_param->name));
    }
    else if (const auto comptime_arg = ast_cast<GenericArgumentCompAst>(c)) {
        value = ast_clone(comptime_arg->val);
    }
    if (auto const *value_as_type = asts::ast_cast<TypeIdentifierAst>(value.get()); value_as_type != nullptr) {
        value = IdentifierAst::from_type(*std::shared_ptr(ast_clone(value_as_type))); // Don't remove "shared_ptr"
    }

    // Create the GenericArgumentCompKeywordAst with the name and value.
    return std::make_unique<GenericArgumentCompKeywordAst>(
        TypeIdentifierAst::from_identifier(*sym.name), nullptr, std::move(value));
}


auto spp::asts::GenericArgumentCompKeywordAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Analyse the value.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(val.get());
    val->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::GenericArgumentCompKeywordAst::stage_8_check_memory(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Check the value for memory issues.
    val->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *val, *val, *sm, true, true, true, true, true, true, meta);
}
