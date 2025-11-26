module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_mangle;
import std;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.asts._fwd;


namespace spp::codegen::mangle {
    SPP_EXP auto mangle_type_name(
        analyse::scopes::TypeSymbol const &type_sym)
        -> std::string;

    SPP_EXP auto mangle_mod_name(
        analyse::scopes::Scope const &mod_scope)
        -> std::string;

    SPP_EXP auto mangle_cmp_name(
        analyse::scopes::Scope const &owner_scope,
        asts::CmpStatementAst const &cmp_stmt)
        -> std::string;

    SPP_EXP auto mangle_fun_name(
        analyse::scopes::Scope const &owner_scope,
        asts::FunctionPrototypeAst const &fun_proto)
        -> std::string;
}
