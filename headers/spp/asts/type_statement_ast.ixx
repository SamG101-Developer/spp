module;
#include <spp/macros.hpp>

export module spp.asts.type_statement_ast;
import spp.asts.statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeStatementAst;
    SPP_EXP_CLS struct UseStatementAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS struct TypeSymbol;
}


/**
 * The TypeStatementAst is used to alias a type to a new name in this scope. It can also use generic parameters for more
 * complex types, such as aliasing vectors, or partially specialized hash maps etc. For example,
 * @code type SecureByteMap[T] = std::collections::HashMap[K=Byte, V=T, A=SecureAlloc[(K, V)]]@endcode
 */
SPP_EXP_CLS struct spp::asts::TypeStatementAst final : StatementAst {
    std::vector<std::unique_ptr<AnnotationAst>> annotations;
    std::unique_ptr<TokenAst> tok_type;
    std::unique_ptr<TypeIdentifierAst> new_type;
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;
    std::unique_ptr<TypeAst> old_type;

    explicit TypeStatementAst(
        decltype(annotations) &&annotations,
        decltype(tok_type) &&tok_type,
        decltype(new_type) new_type,
        decltype(generic_param_group) &&generic_param_group,
        decltype(old_type) old_type);
    ~TypeStatementAst() override;
    auto to_rust() const -> std::string override;
};
