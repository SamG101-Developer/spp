module;
#include <spp/macros.hpp>

export module spp.asts.statement_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The StatementAst class is the base class for all statements in the abstract syntax tree. It is used to represent
 * statements that do not return a value, such as variable declarations and control flow statements.
 */
SPP_EXP_CLS struct spp::asts::StatementAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;

    ~StatementAst() override;

    /**
     * All statement based ASTs are inferred as the Void type, so the method can be implemented here, rather than on
     * every statement AST node.
     * @param sm The scope manager to find the type in.
     * @param meta Associated metadata.
     * @return The Void type, as all statements are void.
     */
    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
