#pragma once
#include <string>

namespace spp::analyse::scopes {
    struct TypeSymbol;
    class Scope;
}

namespace spp::asts {
    struct FunctionPrototypeAst;
}


namespace spp::codegen::mangle {
    auto mangle_type_name(
        analyse::scopes::TypeSymbol const &type_sym)
        -> std::string;

    auto mangle_mod_name(
        analyse::scopes::Scope const &mod_scope)
        -> std::string;

    auto mangle_cmp_name(
        analyse::scopes::Scope const &owner_scope,
        asts::CmpStatementAst const &cmp_stmt)
        -> std::string;

    auto mangle_fun_name(
        analyse::scopes::Scope const &owner_scope,
        asts::FunctionPrototypeAst const &fun_proto)
        -> std::string;
}
