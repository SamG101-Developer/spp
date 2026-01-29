module;
#include <spp/macros.hpp>

export module spp.asts.mixins.type_inferrable_ast;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::asts::mixins {
    SPP_EXP_CLS struct TypeInferrableAst;
}


SPP_EXP_CLS struct spp::asts::mixins::TypeInferrableAst {
    TypeInferrableAst();

    virtual ~TypeInferrableAst();

    virtual auto infer_type(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> = 0;
};
