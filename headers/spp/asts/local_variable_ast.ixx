module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_ast;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableAst : virtual Ast {
protected:
    /**
     * The @c let statement that destructures are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;

    bool m_from_pattern;

public:
    LocalVariableAst();

    ~LocalVariableAst() override;

    virtual auto extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>>;

    virtual auto extract_name() const -> std::shared_ptr<IdentifierAst>;
};
