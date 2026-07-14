module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableAst;
}

SPP_EXP_CLS struct spp::asts::LocalVariableAst : Ast {
    LocalVariableAst();

    ~LocalVariableAst() override;

    SPP_ATTR_NODISCARD virtual auto ExtractName() const -> IdentifierAst*;

    SPP_ATTR_NODISCARD virtual auto ExtractNames() const -> Vec<IdentifierAst*>;

    auto MarkFromCasePattern() -> void;

protected:
    bool _FromCasePattern;
};
