module;
#include <spp/macros.hpp>

module spp.asts.class_member_ast;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::ClassMemberAst::ClassMemberAst() = default;
spp::asts::ClassMemberAst::~ClassMemberAst() = default;
SPP_MOD_END
