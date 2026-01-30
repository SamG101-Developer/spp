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
    bool m_from_case_pattern;

public:
    LocalVariableAst();

    ~LocalVariableAst() override;

    SPP_ATTR_NODISCARD virtual auto extract_names() const
        -> std::vector<std::shared_ptr<IdentifierAst>>;

    SPP_ATTR_NODISCARD virtual auto extract_name() const
        -> std::shared_ptr<IdentifierAst>;

    auto mark_from_case_pattern() -> void;
};
