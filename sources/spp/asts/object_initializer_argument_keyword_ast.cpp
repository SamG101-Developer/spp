module;
#include <spp/macros.hpp>

module spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
ObjectInitializerArgumentKeywordAst::ObjectInitializerArgumentKeywordAst(
  decltype(Name) name,
  decltype(TokAssign) &&tok_assign,
  decltype(Val) &&val) :
  ObjectInitializerArgumentAst(std::move(name), std::move(val)),
  TokAssign(std::move(tok_assign)) {
}

ObjectInitializerArgumentKeywordAst::~ObjectInitializerArgumentKeywordAst() = default;

auto ObjectInitializerArgumentKeywordAst::PosStart() const -> std::size_t {
  // Use the name.
  return Name->PosStart();
}

auto ObjectInitializerArgumentKeywordAst::PosEnd() const -> std::size_t {
  // Use the val.
  return Val->PosEnd();
}

auto ObjectInitializerArgumentKeywordAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<ObjectInitializerArgumentKeywordAst>(
    AstCloneShared(Name),
    AstClone(TokAssign),
    AstClone(Val));
  ast->IsCompilerGenerated = IsCompilerGenerated;
  return ast;
}

auto ObjectInitializerArgumentKeywordAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Name);
  SPP_STRING_APPEND_RAW("=");
  SPP_STRING_APPEND(Val);
  SPP_STRING_END;
}

SPP_MOD_END
