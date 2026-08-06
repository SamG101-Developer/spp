module;
#include <spp/parse/macros.hpp>

module spp.analyse.utils.cmp_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.float_literal_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_size;
import spp.lex.lexer;
import spp.lex.tokens;
import spp.parse.parser_spp;
import spp.utils.strings;
import boost;
import genex;
import sys;

namespace ops {
  template <typename T>
  struct min {
    constexpr auto operator()(const T &x, const T &y) const -> T {
      return std::less<T>{}(x, y) ? x : y;
    }
  };

  template <typename T>
  struct fmin {
    constexpr auto operator()(const T &x, const T &y) const -> T {
      return std::fmin(x, y);
    }
  };

  template <typename T>
  struct max {
    constexpr auto operator()(const T &x, const T &y) const -> T {
      return std::greater<T>{}(x, y) ? x : y;
    }
  };

  template <typename T>
  struct fmax {
    constexpr auto operator()(const T &x, const T &y) const -> T {
      return std::fmax(x, y);
    }
  };

  template <typename T>
  struct cmp {
    constexpr auto operator()(const T &x, const T &y) const -> T {
      return static_cast<T>(std::less<T>{}(x, y) ? -1 : std::greater<T>{}(x, y) ? 1 : 0);
    }
  };

  template <typename T>
  struct fcmp {
    constexpr auto operator()(const T &x, const T &y) const -> T {
      return static_cast<T>(std::less<T>{}(x, y) ? -1 : std::greater<T>{}(x, y) ? 1 : 0);
    }
  };

  template <typename T>
  struct abs {
    constexpr auto operator()(const T &x) const -> T {
      return static_cast<T>(std::llabs(x));
    }
  };

  template <typename T>
  struct fabs {
    constexpr auto operator()(const T &x) const -> T {
      return static_cast<T>(std::fabsl(static_cast<long double>(x)));
    }
  };

  template <typename T>
  struct floor {
    constexpr auto operator()(const T &x) const -> T {
      return static_cast<T>(std::floorl(static_cast<long double>(x)));
    }
  };

  template <typename T>
  struct ceil {
    constexpr auto operator()(const T &x) const -> T {
      return static_cast<T>(std::ceill(static_cast<long double>(x)));
    }
  };

  template <typename T>
  struct trunc {
    constexpr auto operator()(const T &x) const -> T {
      return static_cast<T>(std::truncl(static_cast<long double>(x)));
    }
  };

  template <typename T>
  struct round {
    constexpr auto operator()(const T &x) const -> T {
      return static_cast<T>(std::roundl(static_cast<long double>(x)));
    }
  };

  template <typename T>
  struct frem {
    constexpr auto operator()(const T &x, const T &y) const -> T {
      return std::fmod(x, y);
    }
  };
}

#define SPP_STANDARD_BINARY_BOOL_OP(Op)                                                       \
  const auto cpp_result = Op<bool>()(lhs.CppVal(), rhs.CppVal());                             \
  const auto lex_tok = cpp_result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE; \
  auto tok_ast = MakeUnique<asts::TokenAst>(0uz, lex_tok, spp::lex::tok_to_string(lex_tok));  \
  return MakeUnique<asts::BooleanLiteralAst>(std::move(tok_ast))

#define SPP_STANDARD_UNARY_BOOL_OP(Op)                                                        \
  const auto cpp_result = Op<bool>()(val.CppVal());                                           \
  const auto lex_tok = cpp_result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE; \
  auto tok_ast = MakeUnique<asts::TokenAst>(0uz, lex_tok, spp::lex::tok_to_string(lex_tok));  \
  return MakeUnique<asts::BooleanLiteralAst>(std::move(tok_ast));

#define SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, Ty, CppTy)                                             \
  if (lhs.Type == Ty) {                                                                                        \
    const auto result = Op<CppTy>()(lhs.CppVal<CppTy>(), rhs.CppVal<CppTy>());                               \
    auto val_tok = MakeUnique<asts::TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::format("{}", static_cast<std::size_t>(result))); \
    return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(val_tok), Str(lhs.Type));                 \
  }

#define SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, Ty, CppTy)                              \
  if (lhs.Type == Ty) {                                                                          \
    const auto result = Op<CppTy>()(lhs.CppVal<CppTy>(), rhs.CppVal<CppTy>());                 \
    const auto lex_tok = result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE;    \
    auto tok_ast = MakeUnique<asts::TokenAst>(0uz, lex_tok, spp::lex::tok_to_string(lex_tok)); \
    return MakeUnique<asts::BooleanLiteralAst>(std::move(tok_ast));                            \
  }

#define SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, Ty, CppTy)                                              \
  if (val.Type == Ty) {                                                                                        \
    const auto result = Op<CppTy>()(val.CppVal<CppTy>());                                                    \
    auto val_tok = MakeUnique<asts::TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::format("{}", static_cast<std::size_t>(result))); \
    return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(val_tok), Str(val.Type));                  \
  }

#define SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, Ty, CppTy)                                         \
  if (lhs.Type == Ty) {                                                                                        \
    const auto result = Op<CppTy>()(lhs.CppVal<CppTy>(), rhs.CppVal<CppTy>());                               \
    auto val_tok = MakeUnique<asts::TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::format("{}", static_cast<double>(result))); \
    return asts::FloatLiteralAst::FromSingleTok(nullptr, std::move(val_tok), Str(lhs.Type));                 \
  }

#define SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, Ty, CppTy)                            \
  if (lhs.Type == Ty) {                                                                          \
    const auto result = Op<CppTy>()(lhs.CppVal<CppTy>(), rhs.CppVal<CppTy>());                 \
    const auto lex_tok = result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE;    \
    auto tok_ast = MakeUnique<asts::TokenAst>(0uz, lex_tok, spp::lex::tok_to_string(lex_tok)); \
    return MakeUnique<asts::BooleanLiteralAst>(std::move(tok_ast));                            \
  }

#define SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, Ty, CppTy)                                          \
  if (val.Type == Ty) {                                                                                        \
    const auto result = Op<CppTy>()(val.CppVal<CppTy>());                                                    \
    auto val_tok = MakeUnique<asts::TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::format("{}", static_cast<double>(result))); \
    return asts::FloatLiteralAst::FromSingleTok(nullptr, std::move(val_tok), Str(val.Type));                 \
  }

/*
 * COLLECTION OF "(INT, INT) -> INT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(Op)                        \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s8", std::int8_t)       \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s16", std::int16_t)     \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s32", std::int32_t)     \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s64", std::int64_t)     \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s128", boost::int128_t) \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s256", boost::int256_t) \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "sz", sys::ssize_t)

#define SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(Op)                       \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u8", std::uint8_t)       \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u16", std::uint16_t)     \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u32", std::uint32_t)     \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u64", std::uint64_t)     \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u128", boost::uint128_t) \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u256", boost::uint256_t) \
  SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "uz", std::size_t)

#define SPP_STANDARD_BINARY_INT_OP_RET_INT(Op)  \
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(Op) \
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(Op)

/*
 * COLLECTION OF "(INT, INT) -> BOOL" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(Op)                     \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s8", std::int8_t)    \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s16", std::int16_t)  \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s32", std::int32_t)  \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s64", std::int64_t)  \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "sz", std::ptrdiff_t)

#define SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(Op)                   \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u8", std::uint8_t)   \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u16", std::uint16_t) \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u32", std::uint32_t) \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u64", std::uint64_t) \
  SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "uz", std::size_t)

#define SPP_STANDARD_BINARY_INT_OP_RET_BOOL(Op)    \
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(Op)   \
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(Op)

/*
 * COLLECTION OF "INT -> INT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(Op)                     \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s8", std::int8_t)    \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s16", std::int16_t)  \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s32", std::int32_t)  \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s64", std::int64_t)  \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "sz", std::ptrdiff_t)

#define SPP_STANDARD_UNARY_UNSIGNED_INT_OP_RET_INT(Op)                   \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u8", std::uint8_t)   \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u16", std::uint16_t) \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u32", std::uint32_t) \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u64", std::uint64_t) \
  SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "uz", std::size_t)

#define SPP_STANDARD_UNARY_INT_OP_RET_INT(Op)  \
  SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(Op) \
  SPP_STANDARD_UNARY_UNSIGNED_INT_OP_RET_INT(Op)

/*
 * COLLECTION OF "(FLOAT, FLOAT) -> FLOAT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(Op)                             \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f16", std::float16_t) \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f32", std::float32_t) \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f64", std::float64_t)

/*
 * COLLECTION OF "(FLOAT, FLOAT) -> BOOL" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(Op)                             \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, "f16", std::float16_t) \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, "f32", std::float32_t) \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, "f64", std::float64_t)

/*
 * COLLECTION OF "(FLOAT, FLOAT) -> INT" OPERATIONS FOR STANDARD CMP INTRINSICS. Unlike the int RET_INT handler above,
 * this casts through a signed type so a -1 result (eg from a three-way "cmp") round-trips correctly instead of
 * wrapping to a huge unsigned value. The result is tagged with the bit-width-matched signed int type, since the
 * float type name itself (eg "f32") is not a valid IntegerLiteralAst::Type.
 */
#define SPP_STANDARD_BINARY_FLOAT_OP_RETURN_INT_HANDLER(Op, Ty, CppTy, IntTy)                                     \
  if (lhs.Type == Ty) {                                                                                         \
    const auto result = Op<CppTy>()(lhs.CppVal<CppTy>(), rhs.CppVal<CppTy>());                                \
    auto val_tok = MakeUnique<asts::TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::format("{}", static_cast<std::ptrdiff_t>(result))); \
    return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(val_tok), Str(IntTy));                      \
  }

#define SPP_STANDARD_BINARY_FLOAT_OP_RET_INT(Op)                                    \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_INT_HANDLER(Op, "f16", std::float16_t, "s16") \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_INT_HANDLER(Op, "f32", std::float32_t, "s32") \
  SPP_STANDARD_BINARY_FLOAT_OP_RETURN_INT_HANDLER(Op, "f64", std::float64_t, "s64")

/*
 * COLLECTION OF "FLOAT -> FLOAT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_UNARY_FLOAT_OP(Op)                                       \
  SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f16", std::float16_t) \
  SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f32", std::float32_t) \
  SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f64", std::float64_t)

/*
 * COLLECTION OF "TYPE -> numeric_limits<TYPE>::Limit()" OPERATIONS FOR STANDARD CMP INTRINSICS. The incoming literal's
 * value is unused; only its type tag selects which bound to fold in.
 */
#define SPP_STANDARD_UNARY_FLOAT_LIMIT_HANDLER(Limit, Ty, CppTy)                                                                  \
  if (val.Type == Ty) {                                                                                                           \
    constexpr auto result = std::numeric_limits<CppTy>::Limit();                                                                  \
    auto val_tok = MakeUnique<asts::TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::format("{}", static_cast<double>(result))); \
    return asts::FloatLiteralAst::FromSingleTok(nullptr, std::move(val_tok), Str(val.Type));                                      \
  }

#define SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, Ty, CppTy)                                                                         \
  if (val.Type == Ty) {                                                                                                                \
    constexpr auto result = std::numeric_limits<CppTy>::Limit();                                                                       \
    auto val_tok = MakeUnique<asts::TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::format("{}", static_cast<std::size_t>(result))); \
    return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(val_tok), Str(val.Type));                                            \
  }

#define SPP_STANDARD_UNARY_FLOAT_LIMIT(Limit)                          \
  SPP_STANDARD_UNARY_FLOAT_LIMIT_HANDLER(Limit, "f16", std::float16_t) \
  SPP_STANDARD_UNARY_FLOAT_LIMIT_HANDLER(Limit, "f32", std::float32_t) \
  SPP_STANDARD_UNARY_FLOAT_LIMIT_HANDLER(Limit, "f64", std::float64_t)

#define SPP_STANDARD_UNARY_INT_LIMIT(Limit)                             \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "s8", std::int8_t)        \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "s16", std::int16_t)      \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "s32", std::int32_t)      \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "s64", std::int64_t)      \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "s128", boost::int128_t)  \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "s256", boost::int256_t)  \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "sz", sys::ssize_t)       \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "u8", std::uint8_t)       \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "u16", std::uint16_t)     \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "u32", std::uint32_t)     \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "u64", std::uint64_t)     \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "u128", boost::uint128_t) \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "u256", boost::uint256_t) \
  SPP_STANDARD_UNARY_INT_LIMIT_HANDLER(Limit, "uz", std::size_t)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"

auto spp::analyse::utils::cmp_utils::SetCompTimeAttrValue(
  asts::ObjectInitializerAst const *object,
  asts::Ast *attribute,
  Unique<asts::ExpressionAst> &&value,
  scopes::ScopeManager const *sm)
  -> void {
  // Firstly, we need to split the "attribute" as it may be a dotted path.
  auto attr_path = Vec<Shared<asts::IdentifierAst>>();
  while (true) {
    // Ensure we are looking at a postfix expression.
    const auto postfix = attribute->To<asts::PostfixExpressionAst>();
    if (postfix == nullptr) { break; }

    // Ensure the operator is a runtime member access.
    const auto member_access = postfix->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>();
    if (member_access == nullptr) { break; }

    // Push the member name to the path and continue down the lhs.
    attr_path.push_back(member_access->Name);
    attribute = postfix->Lhs.get();
  }

  // Reverse the attribute path to get the correct order.
  attr_path |= genex::actions::reverse;
  attr_path |= genex::actions::pop_front;

  // For each attribute, check if the initializer exists for tis attribute.
  // For example, if we have x.y.z = 5, and the types are x->X, y->Y, z->Z,
  //  we need to ensure that X has a Y initializer, which has a Z initializer.
  //  each inner initializer may exist, may not, so check and create if needed.
  auto current_obj_init = object;
  auto current_obj_type = object->Type;
  auto current_obj_sym = sm->CurrentScope->GetTypeSymbol(current_obj_type.get());

  for (auto const &attr_name : attr_path) {
    const auto is_final = attr_name == attr_path.Back();
    current_obj_type = current_obj_sym->LinkedScope->GetVarSymbol(attr_name.get())->Type;
    current_obj_sym = current_obj_sym->LinkedScope->GetTypeSymbol(current_obj_type.get());

    // Check if the attribute already exists in the current object initializer.
    const auto found = genex::contains(
      current_obj_init->ArgGroup->GetAllArgs(), *attr_name, [](auto const *x) -> decltype(auto) { return *x->Name; });
    if (found) {
      auto const &arg = **genex::find_if(
        current_obj_init->ArgGroup->GetAllArgs(), [&](auto const *x) { return *x->Name == *attr_name; });
      const auto obj = arg.Val->To<asts::ObjectInitializerAst>();

      // Case: attribute exists, but not as an object initializer. Replace it, but keep old object as the "else"
      // in the initializer, to use all of its other attributes.
      if (obj == nullptr) {
        const auto old_obj_init = current_obj_init;
        auto new_init = is_final
          ? std::move(value)
          : MakeUnique<asts::ObjectInitializerAst>(current_obj_sym->FqName(), nullptr);
        current_obj_init = new_init->To<asts::ObjectInitializerAst>();

        old_obj_init->ArgGroup->Args.EmplaceBack(
          MakeUnique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(new_init)));
        old_obj_init->ArgGroup->Args.EmplaceBack(
          asts::ObjectInitializerArgumentShorthandAst::CreateAutoFillArg(asts::AstClone(arg.Val)));
        continue;
      }

      // Case: attribute exists, and as an object initializer. In this case, just forward into the next
      // initializer.
      if (not is_final) {
        current_obj_init = obj;
        current_obj_type = current_obj_sym->LinkedScope->GetVarSymbol(attr_name.get())->Type;
        current_obj_sym = current_obj_sym->LinkedScope->GetTypeSymbol(current_obj_type.get());
        continue;
      }
      current_obj_init->ArgGroup->Args.EmplaceBack(
        MakeUnique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(value)));
    }

    // Case: attribute does not exist, so create a new object initializer for it.
    else {
      const auto old_obj_init = current_obj_init;
      auto new_init = is_final
        ? std::move(value)
        : MakeUnique<asts::ObjectInitializerAst>(current_obj_sym->FqName(), nullptr);
      current_obj_init = new_init->To<asts::ObjectInitializerAst>();

      old_obj_init->ArgGroup->Args.EmplaceBack(
        MakeUnique<asts::ObjectInitializerArgumentKeywordAst>(AstCloneShared(attr_name), nullptr, std::move(new_init)));
    }
  }
}

auto spp::analyse::utils::cmp_utils::GetCompTimeAttrValue(
  asts::ObjectInitializerAst const *object,
  asts::IdentifierAst const *attribute)
  -> Unique<asts::ExpressionAst> {
  // Check each argument in the object initializer for the target attribute.
  for (auto const &arg : object->ArgGroup->GetAllArgs()) {
    if (*arg->Name == *attribute) { return asts::AstClone(arg->Val); }
  }
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_boolean_and(
  asts::BooleanLiteralAst const &lhs,
  asts::BooleanLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform bitwise AND operation on two boolean literals.
  SPP_STANDARD_BINARY_BOOL_OP(std::logical_and);
}

auto spp::analyse::utils::cmp_utils::std_boolean_ior(
  asts::BooleanLiteralAst const &lhs,
  asts::BooleanLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform bitwise OR operation on two boolean literals.
  SPP_STANDARD_BINARY_BOOL_OP(std::logical_or);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_add(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform addition on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::plus);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_add_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform addition assignment on an integer literal.
  const auto result_literal = std_intrinsics_add(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sub(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform subtraction on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::minus);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sub_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform subtraction assignment on an integer literal.
  const auto result_literal = std_intrinsics_sub(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_mul(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform multiplication on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::multiplies);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_mul_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform multiplication assignment on an integer literal.
  const auto result_literal = std_intrinsics_mul(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sdiv(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed division on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::divides);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sdiv_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform signed division assignment on an integer literal.
  const auto result_literal = std_intrinsics_sdiv(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_udiv(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned division on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::divides);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_udiv_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform unsigned division assignment on an integer literal.
  const auto result_literal = std_intrinsics_udiv(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_srem(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed remainder on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::modulus);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_srem_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform signed remainder assignment on an integer literal.
  const auto result_literal = std_intrinsics_srem(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_urem(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned remainder on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::modulus);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_urem_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform unsigned remainder assignment on an integer literal.
  const auto result_literal = std_intrinsics_urem(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sneg(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed negation on an integer literal.
  SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(std::negate);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shl(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise left shift on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::bit_shl);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shl_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform bitwise left shift assignment on an integer literal.
  const auto result_literal = std_intrinsics_bit_shl(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shr(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise right shift on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::bit_shr);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shr_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform bitwise right shift assignment on an integer literal.
  const auto result_literal = std_intrinsics_bit_shr(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_ior(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise OR on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::bit_or);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_ior_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform bitwise OR assignment on an integer literal.
  const auto result_literal = std_intrinsics_bit_ior(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_and(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise AND on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::bit_and);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_and_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform bitwise AND assignment on an integer literal.
  const auto result_literal = std_intrinsics_bit_and(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_xor(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise XOR on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_INT(std::bit_xor);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_xor_assign(
  asts::IntegerLiteralAst &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> void {
  // Perform bitwise XOR assignment on an integer literal.
  const auto result_literal = std_intrinsics_bit_xor(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform bitwise NOT on an integer literal.
  SPP_STANDARD_UNARY_INT_OP_RET_INT(std::bit_not);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not_assign(
  asts::IntegerLiteralAst &lhs)
  -> void {
  // Perform bitwise NOT assignment on an integer literal.
  const auto result_literal = std_intrinsics_bit_not(lhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.Val = std::move(result_literal->Val);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_abs(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform absolute value on an integer literal.
  SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(ops::abs);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_eq(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform equality comparison on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_BOOL(std::equal_to);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_oeq(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered equality comparison on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(std::equal_to);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ne(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform inequality comparison on two integer literals.
  SPP_STANDARD_BINARY_INT_OP_RET_BOOL(std::not_equal_to);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_one(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered inequality comparison on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(std::not_equal_to);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_slt(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed less-than comparison on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(std::less);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ult(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned less-than comparison on two integer literals.
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(std::less);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_olt(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered less-than comparison on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(std::less);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sle(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed less-than-or-equal comparison on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(std::less_equal);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ule(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned less-than-or-equal comparison on two integer literals.
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(std::less_equal);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ole(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered less-than-or-equal comparison on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(std::less_equal);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sgt(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed greater-than comparison on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(std::greater);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ugt(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned greater-than comparison on two integer literals.
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(std::greater);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ogt(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered greater-than comparison on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(std::greater);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_sge(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform signed greater-than-or-equal comparison on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(std::greater_equal);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_uge(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform unsigned greater-than-or-equal comparison on two integer literals.
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(std::greater_equal);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_oge(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::BooleanLiteralAst> {
  // Perform ordered greater-than-or-equal comparison on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(std::greater_equal);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_max_val(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Return the maximum value for the type of the integer literal.
  SPP_STANDARD_UNARY_INT_LIMIT(max)
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_min_val(
  asts::IntegerLiteralAst const &val)
  -> Unique<asts::IntegerLiteralAst> {
  // Return the minimum value for the type of the integer literal.
  SPP_STANDARD_UNARY_INT_LIMIT(lowest)
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_smax(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed maximum on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(ops::max);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_umax(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned maximum on two integer literals.
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(ops::max);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_smin(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed minimum on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(ops::min);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_umin(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform unsigned minimum on two integer literals.
  SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(ops::min);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_scmp(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed comparison on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(ops::cmp);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ucmp(
  asts::IntegerLiteralAst const &lhs,
  asts::IntegerLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform signed comparison on two integer literals.
  SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(ops::cmp);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fcmp(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::IntegerLiteralAst> {
  // Perform a three-way comparison on two float literals, folding to -1 / 0 / 1.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_INT(ops::fcmp);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fadd(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform addition on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(std::plus);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fadd_assign(
  asts::FloatLiteralAst &lhs,
  asts::FloatLiteralAst const &rhs)
  -> void {
  // Perform addition assignment on a float literal.
  const auto result_literal = std_intrinsics_fadd(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.IntVal = std::move(result_literal->IntVal);
  lhs.FracVal = std::move(result_literal->FracVal);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fsub(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform subtraction on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(std::minus);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fsub_assign(
  asts::FloatLiteralAst &lhs,
  asts::FloatLiteralAst const &rhs)
  -> void {
  // Perform subtraction assignment on a float literal.
  const auto result_literal = std_intrinsics_fsub(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.IntVal = std::move(result_literal->IntVal);
  lhs.FracVal = std::move(result_literal->FracVal);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmul(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform multiplication on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(std::multiplies);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmul_assign(
  asts::FloatLiteralAst &lhs,
  asts::FloatLiteralAst const &rhs)
  -> void {
  // Perform multiplication assignment on a float literal.
  const auto result_literal = std_intrinsics_fmul(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.IntVal = std::move(result_literal->IntVal);
  lhs.FracVal = std::move(result_literal->FracVal);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fdiv(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform division on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(std::divides);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fdiv_assign(
  asts::FloatLiteralAst &lhs,
  asts::FloatLiteralAst const &rhs)
  -> void {
  // Perform division assignment on a float literal.
  const auto result_literal = std_intrinsics_fdiv(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.IntVal = std::move(result_literal->IntVal);
  lhs.FracVal = std::move(result_literal->FracVal);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_frem(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform remainder on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(ops::frem);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_frem_assign(
  asts::FloatLiteralAst &lhs,
  asts::FloatLiteralAst const &rhs)
  -> void {
  // Perform remainder assignment on a float literal.
  const auto result_literal = std_intrinsics_frem(lhs, rhs);
  lhs.TokSign = std::move(result_literal->TokSign);
  lhs.IntVal = std::move(result_literal->IntVal);
  lhs.FracVal = std::move(result_literal->FracVal);
  lhs.Type = std::move(result_literal->Type);
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fneg(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform negation on a float literal.
  SPP_STANDARD_UNARY_FLOAT_OP(std::negate);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fabs(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform absolute value on a float literal.
  SPP_STANDARD_UNARY_FLOAT_OP(ops::fabs);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmax_val(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Get the largest finite value that this float type can hold.
  SPP_STANDARD_UNARY_FLOAT_LIMIT(max);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmin_val(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Get the most negative finite value that this float type can hold.
  SPP_STANDARD_UNARY_FLOAT_LIMIT(lowest);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmax(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform maximum on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(ops::fmax);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fmin(
  asts::FloatLiteralAst const &lhs,
  asts::FloatLiteralAst const &rhs)
  -> Unique<asts::FloatLiteralAst> {
  // Perform minimum on two float literals.
  SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(ops::fmin);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ffloor(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform floor operation on a float literal.
  SPP_STANDARD_UNARY_FLOAT_OP(ops::floor);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fceil(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform ceiling operation on a float literal.
  SPP_STANDARD_UNARY_FLOAT_OP(ops::ceil);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_ftrunc(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform truncation operation on a float literal.
  SPP_STANDARD_UNARY_FLOAT_OP(ops::trunc);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_intrinsics_fround(
  asts::FloatLiteralAst const &val)
  -> Unique<asts::FloatLiteralAst> {
  // Perform round operation on a float literal.
  SPP_STANDARD_UNARY_FLOAT_OP(ops::round);
  return nullptr;
}

auto spp::analyse::utils::cmp_utils::std_num_float_neg_one()
  -> Unique<asts::FloatLiteralAst> {
  // Get "-1.0" as a constant.
  constexpr auto value = "-1.0";
  auto flt = INJECT_CODE(value, parse_literal_float);
  return flt;
}

auto spp::analyse::utils::cmp_utils::std_num_float_zero()
  -> Unique<asts::FloatLiteralAst> {
  // Get "0.0" as a constant.
  constexpr auto value = "0.0";
  auto num = INJECT_CODE(value, parse_literal_float);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_float_one()
  -> Unique<asts::FloatLiteralAst> {
  // Get "1.0" as a constant.
  constexpr auto value = "1.0";
  auto num = INJECT_CODE(value, parse_literal_float);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_neg_one()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "-1" as a constant.
  constexpr auto value = "-1";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_zero()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "0" as a constant.
  constexpr auto value = "0";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_one()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "1" as a constant.
  constexpr auto value = "1";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_num_int_two()
  -> Unique<asts::IntegerLiteralAst> {
  // Get "2" as a constant.
  constexpr auto value = "2";
  auto num = INJECT_CODE(value, parse_literal_integer);
  return num;
}

auto spp::analyse::utils::cmp_utils::std_mem_ops_size_of(
  scopes::ScopeManager const &sm,
  Vec<asts::TypeAst*> const &types)
  -> Unique<asts::IntegerLiteralAst> {
  // Get the size of a type as an integer literal.
  const auto size = codegen::SizeOf(sm, *types[0]);
  auto tok = MakeUnique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(size));
  return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(tok), "uz");
}

auto spp::analyse::utils::cmp_utils::std_mem_ops_align_of(
  scopes::ScopeManager const &sm,
  Vec<asts::TypeAst*> const &types)
  -> Unique<asts::IntegerLiteralAst> {
  // Get the alignment of a type as an integer literal.
  const auto size = codegen::AlignOf(sm, *types[0]);
  auto tok = MakeUnique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(size));
  return MakeUnique<asts::IntegerLiteralAst>(nullptr, std::move(tok), "uz");
}

#pragma GCC diagnostic pop
