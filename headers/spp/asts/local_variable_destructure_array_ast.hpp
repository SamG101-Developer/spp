#ifndef LOCAL_VARIABLE_DESTRUCTURE_ARRAY_AST_HPP
#define LOCAL_VARIABLE_DESTRUCTURE_ARRAY_AST_HPP

#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LocalVariableDestructureArrayAst final : LocalVariableAst {
    SPP_AST_KEY_FUNCTIONS;
    friend struct CasePatternVariantDestructureArrayAst;

private:
    std::vector<std::unique_ptr<LetStatementInitializedAst>> m_new_asts;

public:
    /**
     * The @code [@endcode token that indicates the start of an array destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the array destructuring pattern. This is a list of patterns that will be destructured from the
     * array. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    std::vector<std::unique_ptr<LocalVariableAst>> elems;

    /**
     * The @code ]@endcode token that indicates the end of an array destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the LocalVariableDestructureArrayAst with the arguments matching the members.
     * @param[in] tok_l The @code [@endcode token that indicates the start of an array destructuring pattern.
     * @param[in] elems The elements of the array destructuring pattern.
     * @param[in] tok_r The @code ]@endcode token that indicates the end of an array destructuring pattern.
     */
    LocalVariableDestructureArrayAst(
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elems,
        decltype(tok_r) &&tok_r);

    ~LocalVariableDestructureArrayAst() override;

    auto extract_name() const -> std::shared_ptr<IdentifierAst> override;

    auto extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //LOCAL_VARIABLE_DESTRUCTURE_ARRAY_AST_HPP
