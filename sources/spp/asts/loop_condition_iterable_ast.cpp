#include <spp/asts/expression_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/loop_condition_iterable_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::LoopConditionIterableAst::LoopConditionIterableAst(
    decltype(var) &&var,
    decltype(tok_in) &&tok_in,
    decltype(iterable) &&iterable):
    LoopConditionAst(),
    var(std::move(var)),
    tok_in(std::move(tok_in)),
    iterable(std::move(iterable)) {
}


spp::asts::LoopConditionIterableAst::~LoopConditionIterableAst() = default;


auto spp::asts::LoopConditionIterableAst::pos_start() const -> std::size_t {
    return var->pos_start();
}


auto spp::asts::LoopConditionIterableAst::pos_end() const -> std::size_t {
    return iterable->pos_end();
}


spp::asts::LoopConditionIterableAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(tok_in);
    SPP_STRING_APPEND(iterable);
    SPP_STRING_END;
}


auto spp::asts::LoopConditionIterableAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(tok_in);
    SPP_PRINT_APPEND(iterable);
    SPP_PRINT_END;
}
