module;
#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.abstract_type_ast;
import spp.utils.cache;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
}


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
SPP_EXP_CLS struct spp::asts::TypeAst : PrimaryExpressionAst, mixins::AbstractTypeAst, std::enable_shared_from_this<TypeAst> {
    using PrimaryExpressionAst::PrimaryExpressionAst;

    auto _spp_key_function() const -> void override;

    ~TypeAst() override;

    mutable utils::Cache<analyse::scopes::Scope const*, std::shared_ptr<analyse::scopes::TypeSymbol>> cached_type_symbols;

    mutable std::shared_ptr<TypeAst> m_without_generics_cache;

    mutable std::string m_stringification_cache;

    SPP_ATTR_NODISCARD virtual auto is_type_identifier() const noexcept -> bool { return false; }
};


SPP_MOD_BEGIN
auto spp::asts::TypeAst::_spp_key_function() const -> void {}
SPP_MOD_END
