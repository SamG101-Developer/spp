#include <spp/asts/loop_condition_boolean_ast.hpp>
#include <spp/asts/expression_ast.hpp>


spp::asts::LoopConditionBooleanAst::LoopConditionBooleanAst(
    decltype(cond) &&cond):
    LoopConditionAst(),
    cond(std::move(cond)) {
}


spp::asts::LoopConditionBooleanAst::~LoopConditionBooleanAst() = default;


auto spp::asts::LoopConditionBooleanAst::pos_start() const -> std::size_t {
    return cond->pos_start();
}


auto spp::asts::LoopConditionBooleanAst::pos_end() const -> std::size_t {
    return cond->pos_end();
}


spp::asts::LoopConditionBooleanAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(cond);
    SPP_STRING_END;
}


auto spp::asts::LoopConditionBooleanAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_END;
}
