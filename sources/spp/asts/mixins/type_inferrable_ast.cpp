module;
#include <spp/macros.hpp>

module spp.asts.mixins.type_inferrable_ast;

SPP_MOD_BEGIN
spp::asts::mixins::TypeInferrableAst::TypeInferrableAst() = default;
spp::asts::mixins::TypeInferrableAst::~TypeInferrableAst() = default;

auto spp::asts::mixins::TypeInferrableAst::InferTypeForDisplay(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Default behaviour is to use the normal inference steps.
    return InferType(sm, meta);
}

SPP_MOD_END
