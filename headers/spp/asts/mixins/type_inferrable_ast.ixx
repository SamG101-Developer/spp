module;
#include <spp/macros.hpp>

export module spp.asts.mixins.type_inferrable_ast;
import spp.asts._fwd;
import std;


/// @cond
namespace spp::asts::mixins {
    SPP_EXP_CLS class TypeInferrableAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS class CompilerMetaData;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

/// @endcond


/**
 * A @c TypeInferrableAst is an AST that can infer its type. This is used for ASTs that represent expressions or
 * literals that can have a type determined at compile time. The @c infer_type method is used to infer the type of the
 * AST based on the current scope and metadata.
 */
SPP_EXP_CLS class spp::asts::mixins::TypeInferrableAst {
public:
    TypeInferrableAst();

    virtual ~TypeInferrableAst();

    /**
     * Infer the type from the expression or literal represented by this AST.
     * @param sm The scope manager to use for type inference.
     * @param meta Associated metadata.
     * @return The inferred type of the AST.
     */
    virtual auto infer_type(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> = 0;
};
