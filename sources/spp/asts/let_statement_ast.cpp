module;
#include <spp/macros.hpp>

module spp.asts.let_statement_ast;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::LetStatementAst::LetStatementAst() = default;
spp::asts::LetStatementAst::~LetStatementAst() = default;
SPP_MOD_END
