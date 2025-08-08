#include <algorithm>

#include <spp/asts/expression_ast.hpp>
#include <spp/asts/object_initializer_argument_ast.hpp>
#include <spp/asts/object_initializer_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ObjectInitializerArgumentGroupAst::ObjectInitializerArgumentGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(args) &&args,
    decltype(tok_r) &&tok_r):
    Ast(),
    tok_l(std::move(tok_l)),
    args(std::move(args)),
    tok_r(std::move(tok_r)) {
}


spp::asts::ObjectInitializerArgumentGroupAst::~ObjectInitializerArgumentGroupAst() = default;


auto spp::asts::ObjectInitializerArgumentGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ObjectInitializerArgumentGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::ObjectInitializerArgumentGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(args);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ObjectInitializerArgumentGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(args);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
