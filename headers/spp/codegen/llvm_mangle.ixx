module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_mangle;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS struct TypeSymbol;
}


namespace spp::codegen::mangle {
    SPP_EXP_FUN auto mangle_type_name(
        analyse::scopes::TypeSymbol const &type_sym)
        -> Str;

    SPP_EXP_FUN auto mangle_mod_name(
        analyse::scopes::Scope const &mod_scope)
        -> Str;

    SPP_EXP_FUN auto mangle_cmp_name(
        analyse::scopes::Scope const &owner_scope,
        asts::CmpStatementAst const &cmp_stmt)
        -> Str;

    SPP_EXP_FUN auto mangle_fun_name(
        analyse::scopes::Scope const &owner_scope,
        asts::FunctionPrototypeAst const &fun_proto)
        -> Str;
}
