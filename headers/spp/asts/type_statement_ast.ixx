module;
#include <spp/macros.hpp>

export module spp.asts.type_statement_ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.statement_ast;
import spp.asts.sup_member_ast;
import spp.asts.type_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS struct TypeSymbol;
}

SPP_AST_COMMON_FWD_DECL(TypeStatementAst) {
  SPP_EXP_CLS struct AnnotationAst;
  SPP_EXP_CLS struct GenericParameterGroupAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
  SPP_EXP_CLS struct UseStatementAst;
}

/**
 * The TypeStatementAst is used to alias a type to a new name in this scope. It can also use generic parameters for more
 * complex types, such as aliasing vectors, or partially specialized hash maps etc. For example,
 * @code type SecureByteMap[T] = std::collections::HashMap[K=Byte, V=T, A=SecureAlloc[(K, V)]]@endcode
 */
SPP_EXP_CLS struct spp::asts::TypeStatementAst final :
  StatementAst, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypeStatementAst);

  /**
   * The list of annotations that are applied to this type statement. Typically, access modifiers in this context.
   */
  Vec<Unique<AnnotationAst>> Annotations;

  /**
   * The @c type token that starts this statement.
   */
  Unique<TokenAst> TokType;

  /**
   * The type that this type statement is defining. For example, for @code type Str = std::Str@endcode, the
   * @c new_type is @c Str.
   */
  Shared<TypeIdentifierAst> NewType;

  /**
   * The generic parameter group for the new type. For example,
   * @code type MyVector[T] = Vec[T, A=SomeAlloc]@endcode defines @c T as a generic internal to this type
   * statement only.
   */
  Shared<GenericParameterGroupAst> GnParamGroup;

  /**
   * The @c = token that separates the new type from the old type.
   */
  Unique<TokenAst> TokAssign;

  /**
   * The type this statement aliases, as it was written: the @c std::Str of @code type Str = std::Str@endcode . What
   * it resolves to once the chain of aliases behind it has been followed is not kept here but on the symbol, as
   * @c analyse::scopes::AliasInfo::Resolved - one piece of syntax, one meaning, and neither rewritten to hold the
   * other at some point in the walk.
   */
  Shared<TypeAst> OldType;

  struct {
    Shared<TypeAst> OriginalOldType;
  } Source;

  /**
   * Construct the TypeStatementAst with the arguments matching the members.
   * @param annotations The list of annotations that are applied to this type statement.
   * @param tok_type The @c type token that starts this statement.
   * @param new_type The type that this type statement is defining.
   * @param generic_param_group The generic parameter group for the new type.
   * @param tok_assign The @c = token that separates the new type from the old type.
   * @param old_type The old (fully qualified) type that this type statement is defining.
   */
  TypeStatementAst(
    decltype(Annotations) &&annotations,
    decltype(TokType) &&tok_type,
    decltype(NewType) new_type,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(TokAssign) &&tok_assign,
    decltype(OldType) old_type);

  ~TypeStatementAst() override;

  auto Stage1_PreProcess(
    Ast *ctx)
    -> void override;

  auto Stage2_GenTopLvlScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *)
    -> void override;

  auto Stage3_GenTopLvlAliases(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage5_LoadSupScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage6_PreAnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage10_PreCodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto MarkFromUseStatement()
    -> void;

  SPP_ATTR_NODISCARD auto IsFromUseStatement() const
    -> bool;

private:
  bool _Generated;
  bool _FromUseStatement;
  Shared<analyse::scopes::TypeSymbol> _AliasSym;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeStatementAst)
