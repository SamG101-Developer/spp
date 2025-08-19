#ifndef STRING_LITERAL_AST_HPP
#define STRING_LITERAL_AST_HPP

#include <spp/asts/literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::StringLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The string value of the string literal. This is the actual string that is represented by the literal.
     */
    std::unique_ptr<TokenAst> val;

    /**
     * Construct the StringLiteralAst with the arguments matching the members.
     * @param[in] val The string value of the string literal.
     */
    explicit StringLiteralAst(
        decltype(val) &&val);

    ~StringLiteralAst() override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //STRING_LITERAL_AST_HPP
