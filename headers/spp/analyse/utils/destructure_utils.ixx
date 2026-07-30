module;
#include <spp/macros.hpp>

export module spp.analyse.utils.destructure_utils;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct LocalVariableAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::codegen {
  SPP_EXP_CLS struct LLvmCtx;
}

namespace spp::analyse::utils::destructure_utils {
  constexpr auto kUnmatchableTag = "_UNMATCHABLE";

  SPP_EXP_FUN auto GetNestedBindingIdentifiers(
    Vec<Unique<asts::LocalVariableAst>> const &elems)
    -> Vec<Shared<asts::IdentifierAst>>;

  SPP_EXP_FUN auto UnmatchableSingleIdentifier(
    std::size_t pos)
    -> Shared<asts::IdentifierAst>;

  /**
   * Whether an expression names storage: an identifier, or a chain of runtime member accesses rooted at one, such as
   * @c t, @c t.1 or @c self.pos.0. A destructure indexes such a value directly from every element, because cloning it
   * creates no scopes and no evaluation, and because it keeps partial moves attributed to the root's own symbol.
   * Anything else is a temporary, and gets bound by @ref BindDestructureTemporary instead.
   * @param[in] expr The value being destructured.
   * @return Whether @p expr names storage.
   */
  SPP_EXP_FUN auto IsDestructurePlaceExpression(
    asts::ExpressionAst const &expr)
    -> bool;

  /**
   * Bind the value being destructured to a hidden temporary in the current scope, so that the expanded @c let
   * statements can index the temporary rather than a clone of the value. A destructure expands into one @c let per
   * element, and giving each of them its own clone of the value means the value is analysed once per element: any scope
   * the value creates (a @c case expression, a closure etc) is then duplicated, and the duplicates are orphans that
   * desynchronise the scope iterator in stages 8 and 11. It also evaluates the value once per element at runtime. The
   * caller skips this for values that name storage (see @ref IsDestructurePlaceExpression).
   * @param[in] owner The destructure pattern the temporary belongs to, used to give the temporary a unique name.
   * @param[in] val The value being destructured, already analysed by the owning @c let statement.
   * @param[in] val_type The inferred type of @p val, which becomes the type of the temporary.
   * @param[in, out] sm The scope manager whose current scope the temporary's symbol is added to.
   * @return The name of the temporary, to be indexed by the expanded @c let statements.
   */
  SPP_EXP_FUN auto BindDestructureTemporary(
    asts::Ast const &owner,
    asts::ExpressionAst *val,
    Shared<asts::TypeAst> const &val_type,
    scopes::ScopeManager &sm)
    -> Shared<asts::IdentifierAst>;

  /**
   * Check the memory of the value bound to a destructure's hidden temporary. The temporary holds the only analysis of
   * the value, so this is what traverses the value's scopes in stage 8, and what consumes the value.
   * @param[in] owner The destructure pattern the temporary belongs to, blamed for the move of the value.
   * @param[in] tmp_name The name returned by @ref BindDestructureTemporary.
   * @param[in, out] sm The scope manager to get symbol's memory information from.
   * @param[in, out] meta Metadata to pass between ASTs, holding the value in @c LetStatementValue.
   */
  SPP_EXP_FUN auto DestructureTempStage8(
    asts::Ast const &owner,
    asts::IdentifierAst const &tmp_name,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

  /**
   * Hand the comptime value of the destructured value to the destructure's hidden temporary, so that the expanded
   * @c let statements can index it. The owning @c let statement has already resolved the value into @c CmpResult (a
   * field @c CompilerMetaData::Save does not track), so the value is not resolved a second time here.
   * @param[in] tmp_name The name returned by @ref BindDestructureTemporary.
   * @param[in, out] sm The scope manager to get the temporary's symbol from.
   * @param[in, out] meta Metadata to pass between ASTs, holding the resolved value in @c CmpResult.
   */
  SPP_EXP_FUN auto DestructureTempStage9(
    Shared<asts::IdentifierAst> const &tmp_name,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

  /**
   * Generate the value bound to a destructure's hidden temporary into a stack slot, once, before the expanded @c let
   * statements index it. This is what stops a destructure evaluating its value once per element.
   * @param[in] tmp_name The name returned by @ref BindDestructureTemporary.
   * @param[in, out] sm The scope manager to get the temporary's symbol from.
   * @param[in, out] meta Metadata to pass between ASTs, holding the value in @c LetStatementValue.
   * @param[in, out] ctx The LLVM context to generate code into.
   */
  SPP_EXP_FUN auto DestructureTempStage11(
    Shared<asts::IdentifierAst> const &tmp_name,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> void;
}
