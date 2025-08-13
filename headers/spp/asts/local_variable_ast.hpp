#ifndef LOCAL_VARIABLE_AST_HPP
#define LOCAL_VARIABLE_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LocalVariableAst : virtual Ast {
public:
    LocalVariableAst();

    ~LocalVariableAst() override;

protected:
    /**
     * The @c let statement that destructures are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;

    bool m_from_pattern;

public:
    virtual auto extract_names() const -> std::vector<IdentifierAst*>;

    virtual auto extract_name() const -> IdentifierAst*;
};


#endif //LOCAL_VARIABLE_AST_HPP
