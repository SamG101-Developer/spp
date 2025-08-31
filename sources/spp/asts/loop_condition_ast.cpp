#include <spp/asts/loop_condition_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>


auto spp::asts::LoopConditionAst::infer_type(
    ScopeManager *,
    mixins::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Loop conditions are always boolean (analysis prevents non-boolean expressions).
    return generate::common_types::boolean_type(pos_start());
}


spp::asts::LoopConditionAst::~LoopConditionAst() = default;
