#pragma once
#include <spp/asts/ast.hpp>

#include <spp/asts/mixins/type_inferrable.hpp>


/**
 * The StatementAst class is the base class for all statements in the abstract syntax tree. It is used to represent
 * statements that do not return a value, such as variable declarations and control flow statements.
 */
struct spp::asts::StatementAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;

    /**
     * All statement based ASTs are inferred as the Void type, so the method can be implemented here, rather than on
     * every statement AST node.
     * @param sm The scope manager to find the type in.
     * @param meta Associated metadata.
     * @return The Void type, as all statements are void.
     */
    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
