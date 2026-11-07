module;
#include <spp/macros.hpp>

module spp.asts.primary_expression_ast;

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::PrimaryExpressionAst::PrimaryExpressionAst() = default;
spp::asts::PrimaryExpressionAst::~PrimaryExpressionAst() = default;
SPP_MOD_END
