#include <algorithm>

#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/token_ast.hpp>


template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst(
    decltype(tok_l) &&tok_l,
    decltype(members) &&members,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    members(std::move(members)),
    tok_r(std::move(tok_r)) {
}


template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst() :
    tok_l(nullptr),
    members(),
    tok_r(nullptr) {
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


template <typename T>
spp::asts::InnerScopeAst<T>::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(members);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


template <typename T>
auto spp::asts::InnerScopeAst<T>::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(members);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::ClassMemberAst>>;
template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::FunctionMemberAst>>;
template struct spp::asts::InnerScopeAst<std::unique_ptr<spp::asts::SupMemberAst>>;
