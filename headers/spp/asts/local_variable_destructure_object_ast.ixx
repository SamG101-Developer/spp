module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_object_ast;
import spp.asts._fwd;
import spp.asts.local_variable_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureObjectAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureObjectAst final : LocalVariableAst {
    friend struct spp::asts::CasePatternVariantDestructureObjectAst;

private:
    std::vector<std::unique_ptr<LetStatementInitializedAst>> m_new_asts;

public:
    /**
     * The type of the object being destructured. This is used to determine the type of the destructured elements (by
     * attribute type inference)
     */
    std::shared_ptr<TypeAst> type;

    /**
     * The @code (@endcode token that indicates the start of a object destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the object destructuring pattern. This is a list of patterns that will be destructured from the
     * object. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    std::vector<std::unique_ptr<LocalVariableAst>> elems;

    /**
     * The @code )@endcode token that indicates the end of an object destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the LocalVariableDestructureObjectAst with the arguments matching the members.
     * @param[in] type The type of the object being destructured.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a object destructuring pattern.
     * @param[in] elems The elements of the object destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a object destructuring pattern.
     */
    LocalVariableDestructureObjectAst(
        decltype(type) &&type,
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elems,
        decltype(tok_r) &&tok_r);

    ~LocalVariableDestructureObjectAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto extract_name() const -> std::shared_ptr<IdentifierAst> override;

    auto extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
