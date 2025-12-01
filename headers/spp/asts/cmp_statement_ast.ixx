module;
#include <spp/macros.hpp>

export module spp.asts.cmp_statement_ast;
import spp.asts.annotation_ast;
import spp.asts.statement_ast;
import spp.asts.expression_ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.asts.token_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The CmpStatementAst represents a compile time definition statement at either the module or superimposition level. It
 * is analogous to Rust's "const" statement.
 */
SPP_EXP_CLS struct spp::asts::CmpStatementAst final : StatementAst, ModuleMemberAst, SupMemberAst, mixins::VisibilityEnabledAst {
    /**
     * The list of annotations that are applied to this cmp statement. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The token that represents the @c cmp keyword in the cmp statement. This is used to indicate that a compile time
     * definition is being made.
     */
    std::unique_ptr<TokenAst> tok_cmp;

    /**
     * The name of the cmp statement. This is the identifier that is used to refer to the compile time definition, and
     * must be unique within the scope.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * The token that represents the colon @c : in the cmp statement definition. This separates the name from the type.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The type of the cmp statement. This is the type that the compile time definition will hold, and must be
     * specified. Needs to be specified rather than inferred, because the type must be known at an early stage that
     * needs to be completed before type-inference can be considered.
     */
    std::shared_ptr<TypeAst> type;

    /**
     * The token that represents the assignment operator @c = in the cmp statement definition. This separates the type
     * from the value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The value of the cmp statement. This is the expression that will be evaluated at compile time, and must be
     * constant. It can be any expression that is valid in a compile time context, such as a literal or an object
     * initialization that only uses compile time values.
     */
    std::unique_ptr<ExpressionAst> value;

    /**
     * Construct the CmpStatementAst with the arguments matching the members.
     * @param[in] annotations The list of annotations that are applied to this cmp statement.
     * @param[in] tok_cmp The token that represents the @c cmp keyword in the cmp statement.
     * @param[in] name The name of the cmp statement.
     * @param[in] tok_colon The token that represents the colon @c : in the cmp statement definition.
     * @param[in] type The type of the cmp statement.
     * @param[in] tok_assign The token that represents the assignment operator @c = in the cmp statement definition.
     * @param[in] value The value of the cmp statement.
     */
    CmpStatementAst(
        decltype(annotations) &&annotations,
        decltype(tok_cmp) &&tok_cmp,
        decltype(name) &&name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type,
        decltype(tok_assign) &&tok_assign,
        decltype(value) &&value);

    ~CmpStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};


spp::asts::CmpStatementAst::~CmpStatementAst() = default;
