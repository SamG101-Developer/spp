#include <algorithm>

#include <spp/asts/case_pattern_variant_destructure_tuple_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/local_variable_destructure_tuple_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantDestructureTupleAst::CasePatternVariantDestructureTupleAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
}


spp::asts::CasePatternVariantDestructureTupleAst::~CasePatternVariantDestructureTupleAst() = default;


auto spp::asts::CasePatternVariantDestructureTupleAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::CasePatternVariantDestructureTupleAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::CasePatternVariantDestructureTupleAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantDestructureTupleAst>(
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::CasePatternVariantDestructureTupleAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureTupleAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantDestructureTupleAst::convert_to_variable(
    mixins::CompilerMetaData *meta)
    -> std::unique_ptr<LocalVariableAst> {
    // Recursively map the elements to their local variable counterparts.
    auto mapped_elems = elems
        | genex::views::transform([meta](auto &&x) { return x->convert_to_variable(meta); })
        | genex::views::transform([](auto &&x) { return ast_cast<LocalVariableAst>(std::move(x)); })
        | genex::views::to<std::vector>();

    // Create the final local variable wrapping, tag it and return it.
    auto var = std::make_unique<LocalVariableDestructureTupleAst>(nullptr, std::move(mapped_elems), nullptr);
    var->m_from_pattern = true;
    return var;
}


auto spp::asts::CasePatternVariantDestructureTupleAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Create the new variable from the pattern in the patterns scope.
    auto var = convert_to_variable(meta);
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, ast_clone(meta->case_condition));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantDestructureTupleAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Forward memory checking to the mapped let statement.
    m_mapped_let->stage_8_check_memory(sm, meta);
}
