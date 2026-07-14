module;
#include <spp/macros.hpp>

export module spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::mixins {
    SPP_EXP_CLS struct TypeInferrableAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::mixins::TypeInferrableAst {
    TypeInferrableAst();

    virtual ~TypeInferrableAst();

    virtual auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* = 0;

    auto ResetInferredTypeCache() const -> void;

protected:
    mutable Unique<TypeAst> _CachedInferredType;
};
