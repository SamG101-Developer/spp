module;
#include <spp/macros.hpp>

module spp.asts.mixins.type_inferrable_ast;
import spp.asts.type_ast;

SPP_MOD_BEGIN
spp::asts::mixins::TypeInferrableAst::TypeInferrableAst() :
    _CachedInferredType(nullptr) {
}

spp::asts::mixins::TypeInferrableAst::~TypeInferrableAst() = default;

auto spp::asts::mixins::TypeInferrableAst::ResetInferredTypeCache() const -> void {
    _CachedInferredType = nullptr;
}

SPP_MOD_END
