module;
#include <spp/macros.hpp>

export module spp.asts.cmp_statement_ast;
import spp.asts.module_member_ast;
import spp.asts.statement_ast;
import spp.asts.sup_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct UseStatementVariableAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

/**
 * The CmpStatementAst represents a compile time definition statement at either the module or superimposition level. It
 * is analogous to Rust's "const" statement.
 */
SPP_EXP_CLS struct spp::asts::CmpStatementAst final : StatementAst, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
    friend struct UseStatementVariableAst;
    // Todo: Copy the "_Generated" logic from the "UseStatementAst" and add local insertions into testing?

    /**
     * The list of annotations that are applied to this cmp statement. Typically, access modifiers in this context.
     */
    Vec<Unique<AnnotationAst>> Annotations;

    /**
     * The token that represents the @c cmp keyword in the cmp statement. This is used to indicate that a compile time
     * definition is being made.
     */
    Unique<TokenAst> TokCmp;

    /**
     * The name of the cmp statement. This is the identifier that is used to refer to the compile time definition, and
     * must be unique within the scope.
     */
    Unique<IdentifierAst> Name;

    /**
     * The token that represents the colon @c : in the cmp statement definition. This separates the name from the type.
     */
    Unique<TokenAst> TokColon;

    /**
     * The type of the cmp statement. This is the type that the compile time definition will hold, and must be
     * specified. Needs to be specified rather than inferred, because the type must be known at an early stage that
     * needs to be completed before type-inference can be considered.
     */
    Unique<TypeAst> Type;

    /**
     * The token that represents the assignment operator @c = in the cmp statement definition. This separates the type
     * from the value.
     */
    Unique<TokenAst> TokAssign;

    /**
     * The value of the cmp statement. This is the expression that will be evaluated at compile time, and must be
     * constant. It can be any expression that is valid in a compile time context, such as a literal or an object
     * initialization that only uses compile time values.
     */
    Unique<ExpressionAst> Value;

    struct {
        Unique<TypeAst> OriginalType;
    } Source;

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
        decltype(Annotations) &&annotations,
        decltype(TokCmp) &&tok_cmp,
        decltype(Name) &&name,
        decltype(TokColon) &&tok_colon,
        decltype(Type) type,
        decltype(TokAssign) &&tok_assign,
        decltype(Value) &&value);

    ~CmpStatementAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *) -> void override;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage10_PreCodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto MarkFromUseStatement() -> void;

    SPP_ATTR_NODISCARD auto IsFromUseStatement() const -> bool;

private:
    bool _FromUseStatement = false;
    analyse::scopes::VariableSymbol *_AliasSym;
};
