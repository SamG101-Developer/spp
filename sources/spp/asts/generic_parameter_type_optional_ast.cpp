#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/generic_parameter_type_inline_constraints_ast.hpp>
#include <spp/asts/generic_parameter_type_optional_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterTypeOptionalAst::GenericParameterTypeOptionalAst(
    decltype(name) &&name,
    decltype(constraints) &&constraints,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val) :
    GenericParameterTypeAst(std::move(name), std::move(constraints)),
    tok_assign(std::move(tok_assign)),
    default_val(std::move(default_val)) {
}


spp::asts::GenericParameterTypeOptionalAst::~GenericParameterTypeOptionalAst() = default;


auto spp::asts::GenericParameterTypeOptionalAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericParameterTypeOptionalAst::pos_end() const -> std::size_t {
    return default_val->pos_end();
}


auto spp::asts::GenericParameterTypeOptionalAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterTypeOptionalAst>(
        ast_clone(name),
        ast_clone(constraints),
        ast_clone(tok_assign),
        ast_clone(default_val));
}


spp::asts::GenericParameterTypeOptionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(constraints);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterTypeOptionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(constraints);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(default_val);
    SPP_PRINT_END;
}


auto spp::asts::GenericParameterTypeOptionalAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Handle the default type.
    default_val->stage_7_analyse_semantics(sm, meta);
    default_val = sm->current_scope->get_type_symbol(*default_val)->fq_name()->with_convention(ast_clone(default_val->get_convention()));
}


auto spp::asts::GenericParameterTypeOptionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the name and default value of the generic type parameter.
    GenericParameterTypeAst::stage_7_analyse_semantics(sm, meta);
    default_val->stage_7_analyse_semantics(sm, meta);
}
