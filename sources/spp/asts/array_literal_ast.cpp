module;
#include <spp/macros.hpp>

module spp.asts.array_literal_ast;

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::ArrayLiteralAst::ArrayLiteralAst() = default;
spp::asts::ArrayLiteralAst::~ArrayLiteralAst() = default;
SPP_MOD_END
