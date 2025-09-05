#pragma once

#include <spp/asts/_fwd.hpp>


namespace spp::asts::mixins {
    class TypeInferrableAst;
    struct CompilerMetaData;
}


namespace spp::analyse::scopes {
    class ScopeManager;
}


/**
 * A @c TypeInferrableAst is an AST that can infer its type. This is used for ASTs that represent expressions or
 * literals that can have a type determined at compile time. The @c infer_type method is used to infer the type of the
 * AST based on the current scope and metadata.
 */
class spp::asts::mixins::TypeInferrableAst {
public:
    TypeInferrableAst();

    virtual ~TypeInferrableAst();

    /**
     * Infer the type from the expression or literal represented by this AST.
     * @param sm The scope manager to use for type inference.
     * @param meta Associated metadata.
     * @return The inferred type of the AST.
     */
    virtual auto infer_type(analyse::scopes::ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> = 0;
};
