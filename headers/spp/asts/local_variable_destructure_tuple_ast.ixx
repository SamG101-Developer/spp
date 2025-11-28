module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_tuple_ast;
import spp.asts._fwd;
import spp.asts.local_variable_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureTupleAst final : LocalVariableAst {
    friend struct CasePatternVariantDestructureTupleAst;

private:
    std::vector<std::unique_ptr<LetStatementInitializedAst>> m_new_asts;

public:
    /**
     * The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the tuple destructuring pattern. This is a list of patterns that will be destructured from the
     * tuple. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    std::vector<std::unique_ptr<LocalVariableAst>> elems;

    /**
     * The @code )@endcode token that indicates the end of an tuple destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the LocalVariableDestructureTupleAst with the arguments matching the members.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     * @param[in] elems The elements of the tuple destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a tuple destructuring pattern.
     */
    LocalVariableDestructureTupleAst(
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elems,
        decltype(tok_r) &&tok_r);

    ~LocalVariableDestructureTupleAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto extract_name() const -> std::shared_ptr<IdentifierAst> override;

    auto extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
