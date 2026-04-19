module spp.analyse.utils.cmp_utils;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.scope_utils;
import spp.asts;
import spp.asts.utils;
import spp.lex;
import spp.utils.strings;
import genex;
import opex.ops;

#define cmp_abs 0 <_cmp_abs_>
#define cmp_smax <_cmp_smax_>
#define cmp_umax <_cmp_umax_>
#define cmp_smin <_cmp_smin_>
#define cmp_umin <_cmp_umin_>
#define cmp_fabs 0 <_cmp_fabs_>
#define cmp_fmax <_cmp_fmax_>
#define cmp_fmin <_cmp_fmin_>
#define cmp_ffloor 0 <_cmp_ffloor_>
#define cmp_fceil 0 <_cmp_fceil_>
#define cmp_ftrunc 0 <_cmp_ftrunc_>
#define cmp_fround 0 <_cmp_fround_>
#define cmp_frem <_cmp_frem_>


#define SPP_STANDARD_BINARY_BOOL_OP(Op)                                                            \
    const auto cpp_result = lhs.cpp_value() Op rhs.cpp_value();                                    \
    const auto lex_tok = cpp_result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE;    \
    auto tok_ast = std::make_unique<asts::TokenAst>(0, lex_tok, spp::lex::tok_to_string(lex_tok)); \
    return std::make_unique<asts::BooleanLiteralAst>(std::move(tok_ast))


#define SPP_STANDARD_UNARY_BOOL_OP(Op)                                                             \
    const auto cpp_result = Op val.cpp_value();                                                    \
    const auto lex_tok = cpp_result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE;    \
    auto tok_ast = std::make_unique<asts::TokenAst>(0, lex_tok, spp::lex::tok_to_string(lex_tok)); \
    return std::make_unique<asts::BooleanLiteralAst>(std::move(tok_ast));


#define SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, Ty, CppTy)                                                 \
    if (lhs.type == Ty) {                                                                                            \
        const auto result = lhs.cpp_value<CppTy>() Op rhs.cpp_value<CppTy>();                                        \
        auto val_tok = std::make_unique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::format("{}", result)); \
        return std::make_unique<asts::IntegerLiteralAst>(nullptr,  std::move(val_tok), std::string(lhs.type));       \
    }

#define SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, Ty, CppTy)                                  \
    if (lhs.type == Ty) {                                                                              \
        const auto result = lhs.cpp_value<CppTy>() Op rhs.cpp_value<CppTy>();                          \
        const auto lex_tok = result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE;        \
        auto tok_ast = std::make_unique<asts::TokenAst>(0, lex_tok, spp::lex::tok_to_string(lex_tok)); \
        return std::make_unique<asts::BooleanLiteralAst>(std::move(tok_ast));                          \
    }

#define SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, Ty, CppTy)                                               \
    if (val.type == Ty) {                                                                                         \
        const auto result = Op val.cpp_value<CppTy>();                                                            \
        auto val_tok = std::make_unique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::to_string(result)); \
        return std::make_unique<asts::IntegerLiteralAst>(nullptr, std::move(val_tok), std::string(val.type));     \
    }

#define SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, Ty, CppTy)                                             \
    if (lhs.type == Ty) {                                                                                            \
        const auto result = lhs.cpp_value<CppTy>() Op rhs.cpp_value<CppTy>();                                        \
        auto val_tok = std::make_unique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::format("{}", result)); \
        return asts::FloatLiteralAst::from_single_token(nullptr, std::move(val_tok), std::string(lhs.type));         \
    }

#define SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, Ty, CppTy)                                \
    if (lhs.type == Ty) {                                                                              \
        const auto result = lhs.cpp_value<CppTy>() Op rhs.cpp_value<CppTy>();                          \
        const auto lex_tok = result ? lex::SppTokenType::KW_TRUE : lex::SppTokenType::KW_FALSE;        \
        auto tok_ast = std::make_unique<asts::TokenAst>(0, lex_tok, spp::lex::tok_to_string(lex_tok)); \
        return std::make_unique<asts::BooleanLiteralAst>(std::move(tok_ast));                          \
    }

#define SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, Ty, CppTy)                                             \
    if (val.type == Ty) {                                                                                            \
        const auto result = Op val.cpp_value<CppTy>();                                                              \
        auto val_tok = std::make_unique<asts::TokenAst>(0, lex::SppTokenType::LX_NUMBER, std::format("{}", result)); \
        return asts::FloatLiteralAst::from_single_token(nullptr, std::move(val_tok), std::string(val.type));         \
    }


/*
 * COLLECTION OF "(INT, INT) -> INT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(Op)                       \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s8", std::int8_t)    \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s16", std::int16_t)  \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s32", std::int32_t)  \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "s64", std::int64_t)  \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "sz", std::ptrdiff_t)

#define SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(Op)                     \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u8", std::uint8_t)   \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u16", std::uint16_t) \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u32", std::uint32_t) \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "u64", std::uint64_t) \
    SPP_STANDARD_BINARY_INT_OP_RETURN_INT_HANDLER(Op, "uz", std::size_t)

#define SPP_STANDARD_BINARY_INT_OP_RET_INT(Op)      \
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(Op)   \
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(Op)


/*
 * COLLECTION OF "(INT, INT) -> BOOL" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(Op)                       \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s8", std::int8_t)    \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s16", std::int16_t)  \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s32", std::int32_t)  \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "s64", std::int64_t)  \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "sz", std::ptrdiff_t)

#define SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(Op)                     \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u8", std::uint8_t)   \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u16", std::uint16_t) \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u32", std::uint32_t) \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "u64", std::uint64_t) \
    SPP_STANDARD_BINARY_INT_OP_RETURN_BOOL_HANDLER(Op, "uz", std::size_t)

#define SPP_STANDARD_BINARY_INT_OP_RET_BOOL(Op)      \
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(Op)   \
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(Op)


/*
 * COLLECTION OF "INT -> INT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(Op)                       \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s8", std::int8_t)    \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s16", std::int16_t)  \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s32", std::int32_t)  \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "s64", std::int64_t)  \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "sz", std::ptrdiff_t)

#define SPP_STANDARD_UNARY_UNSIGNED_INT_OP_RET_INT(Op)                     \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u8", std::uint8_t)   \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u16", std::uint16_t) \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u32", std::uint32_t) \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "u64", std::uint64_t) \
    SPP_STANDARD_UNARY_INT_OP_RETURN_INT_HANDLER(Op, "uz", std::size_t)

#define SPP_STANDARD_UNARY_INT_OP_RET_INT(Op)    \
    SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(Op) \
    SPP_STANDARD_UNARY_UNSIGNED_INT_OP_RET_INT(Op)


/*
 * COLLECTION OF "(FLOAT, FLOAT) -> FLOAT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(Op)                               \
    SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f16", std::float16_t) \
    SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f32", std::float32_t) \
    SPP_STANDARD_BINARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f64", std::float64_t)


/*
 * COLLECTION OF "(FLOAT, FLOAT) -> BOOL" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(Op)                               \
    SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, "f16", std::float16_t) \
    SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, "f32", std::float32_t) \
    SPP_STANDARD_BINARY_FLOAT_OP_RETURN_BOOL_HANDLER(Op, "f64", std::float64_t)

/*
 * COLLECTION OF "FLOAT -> FLOAT" OPERATIONS FOR STANDARD CMP INTRINSICS
 */
#define SPP_STANDARD_UNARY_FLOAT_OP(Op)                                         \
    SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f16", std::float16_t) \
    SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f32", std::float32_t) \
    SPP_STANDARD_UNARY_FLOAT_OP_RETURN_FLOAT_HANDLER(Op, "f64", std::float64_t)


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"


auto spp::analyse::utils::cmp_utils::set_attribute_value(
    asts::ObjectInitializerAst *object,
    asts::Ast *attribute,
    std::unique_ptr<asts::ExpressionAst> &&value,
    scopes::ScopeManager const *sm)
    -> void {
    // Firstly, we need to split the "attribute" as it may be a dotted path.
    auto attr_path = std::vector<std::shared_ptr<asts::IdentifierAst>>();
    while (true) {
        // Ensure we are looking at a postfix expression.
        const auto postfix = attribute->to<asts::PostfixExpressionAst>();
        if (postfix == nullptr) { break; }

        // Ensure the operator is a runtime member access.
        const auto member_access = postfix->op->to<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>();
        if (member_access == nullptr) { break; }

        // Push the member name to the path and continue down the lhs.
        attr_path.push_back(member_access->name);
        attribute = postfix->lhs.get();
    }

    // Reverse the attribute path to get the correct order.
    attr_path |= genex::actions::reverse;
    attr_path |= genex::actions::pop_front;

    // For each attribute, check if the initializer exists for tis attribute.
    // For example, if we have x.y.z = 5, and the types are x->X, y->Y, z->Z,
    //  we need to ensure that X has a Y initializer, which has a Z initializer.
    //  each inner initializer may exist, may not, so check and create if needed.
    auto current_obj_init = object;
    auto current_obj_type = object->type;
    auto current_obj_sym = scope_utils::get_type_symbol(*sm->current_scope, current_obj_type);

    for (auto const &attr_name : attr_path) {
        const auto is_final = attr_name == attr_path.back();
        current_obj_type = scope_utils::get_var_symbol(*current_obj_sym->scope, attr_name)->type;
        current_obj_sym = scope_utils::get_type_symbol(*current_obj_sym->scope, current_obj_type);

        // Check if the attribute already exists in the current object initializer.
        const auto found = genex::contains(current_obj_init->arg_group->get_all_args(), *attr_name, [](auto const *x) -> decltype(auto) { return *x->name; });
        if (found) {
            auto const &arg = **genex::find_if(current_obj_init->arg_group->get_all_args(), [&](auto const *x) { return *x->name == *attr_name; });
            const auto obj = arg.val->to<asts::ObjectInitializerAst>();

            // Case: attribute exists, but not as an object initializer. Replace it, but keep old object as the "else"
            // in the initializer, to use all of its other attributes.
            if (obj == nullptr) {
                const auto old_obj_init = current_obj_init;
                auto new_init = is_final
                    ? std::move(value)
                    : std::make_unique<asts::ObjectInitializerAst>(current_obj_sym->fq_name(), nullptr);
                current_obj_init = new_init->to<asts::ObjectInitializerAst>();

                old_obj_init->arg_group->args.emplace_back(
                    std::make_unique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(new_init)));
                old_obj_init->arg_group->args.emplace_back(
                    asts::ObjectInitializerArgumentShorthandAst::create_autofill(asts::ast_clone(arg.val)));
                continue;
            }

            // Case: attribute exists, and as an object initializer. In this case, just forward into the next
            // initializer.
            if (not is_final) {
                current_obj_init = obj;
                current_obj_type = scope_utils::get_var_symbol(*current_obj_sym->scope, attr_name)->type;
                current_obj_sym = scope_utils::get_type_symbol(*current_obj_sym->scope, current_obj_type);
                continue;
            }
            current_obj_init->arg_group->args.emplace_back(
                std::make_unique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(value)));
        }

        // Case: attribute does not exist, so create a new object initializer for it.
        else {
            const auto old_obj_init = current_obj_init;
            auto new_init = is_final
                ? std::move(value)
                : std::make_unique<asts::ObjectInitializerAst>(current_obj_sym->fq_name(), nullptr);
            current_obj_init = new_init->to<asts::ObjectInitializerAst>();

            old_obj_init->arg_group->args.emplace_back(
                std::make_unique<asts::ObjectInitializerArgumentKeywordAst>(ast_clone(attr_name), nullptr, std::move(new_init)));
        }
    }
}


auto spp::analyse::utils::cmp_utils::get_attribute_value(
    asts::ObjectInitializerAst const *object,
    asts::IdentifierAst const *attribute)
    -> std::unique_ptr<asts::ExpressionAst> {
    // Check each argument in the object initializer for the target attribute.
    for (auto const &arg : object->arg_group->get_all_args()) {
        if (*arg->name == *attribute) { return asts::ast_clone(arg->val); }
    }
    return nullptr;
}



auto spp::analyse::utils::cmp_utils::std_boolean_bit_and(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform bitwise AND operation on two boolean literals.
    SPP_STANDARD_BINARY_BOOL_OP(&);
}


auto spp::analyse::utils::cmp_utils::std_boolean_bit_ior(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform bitwise OR operation on two boolean literals.
    SPP_STANDARD_BINARY_BOOL_OP(|);
}


auto spp::analyse::utils::cmp_utils::std_boolean_bit_xor(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform bitwise XOR operation on two boolean literals.
    SPP_STANDARD_BINARY_BOOL_OP(^);
}


auto spp::analyse::utils::cmp_utils::std_boolean_bit_not(
    asts::BooleanLiteralAst const &val)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform bitwise NOT operation on a boolean literal.
    SPP_STANDARD_UNARY_BOOL_OP(!);
}


auto spp::analyse::utils::cmp_utils::std_boolean_and(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform logical AND operation on two boolean literals.
    SPP_STANDARD_BINARY_BOOL_OP(&&);
}


auto spp::analyse::utils::cmp_utils::std_boolean_ior(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform logical OR operation on two boolean literals.
    SPP_STANDARD_BINARY_BOOL_OP(||);
}


auto spp::analyse::utils::cmp_utils::std_boolean_eq(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform equality comparison on two boolean literals.
    SPP_STANDARD_BINARY_BOOL_OP(==);
}


auto spp::analyse::utils::cmp_utils::std_boolean_ne(
    asts::BooleanLiteralAst const &lhs,
    asts::BooleanLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform inequality comparison on two boolean literals.
    SPP_STANDARD_BINARY_BOOL_OP(!=);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_add(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform addition on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(+);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_add_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform addition assignment on an integer literal.
    const auto result_literal = std_intrinsics_add(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sub(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform subtraction on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(-);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sub_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform subtraction assignment on an integer literal.
    const auto result_literal = std_intrinsics_sub(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_mul(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform multiplication on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(*);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_mul_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform multiplication assignment on an integer literal.
    const auto result_literal = std_intrinsics_mul(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sdiv(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform signed division on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(/);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sdiv_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform signed division assignment on an integer literal.
    const auto result_literal = std_intrinsics_sdiv(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_udiv(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform unsigned division on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(/);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_udiv_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform unsigned division assignment on an integer literal.
    const auto result_literal = std_intrinsics_udiv(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_srem(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform signed remainder on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(%);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_srem_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform signed remainder assignment on an integer literal.
    const auto result_literal = std_intrinsics_srem(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_urem(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform unsigned remainder on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(%);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_urem_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform unsigned remainder assignment on an integer literal.
    const auto result_literal = std_intrinsics_urem(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sneg(
    asts::IntegerLiteralAst const &val)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform signed negation on an integer literal.
    SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(-);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shl(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform bitwise left shift on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(<<);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shl_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform bitwise left shift assignment on an integer literal.
    const auto result_literal = std_intrinsics_bit_shl(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shr(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform bitwise right shift on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(>>);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_shr_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform bitwise right shift assignment on an integer literal.
    const auto result_literal = std_intrinsics_bit_shr(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_ior(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform bitwise OR on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(|);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_ior_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform bitwise OR assignment on an integer literal.
    const auto result_literal = std_intrinsics_bit_ior(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_and(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform bitwise AND on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(&);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_and_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform bitwise AND assignment on an integer literal.
    const auto result_literal = std_intrinsics_bit_and(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_xor(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform bitwise XOR on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_INT(^);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_xor_assign(
    asts::IntegerLiteralAst &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> void {
    // Perform bitwise XOR assignment on an integer literal.
    const auto result_literal = std_intrinsics_bit_xor(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not(
    asts::IntegerLiteralAst const &val)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform bitwise NOT on an integer literal.
    SPP_STANDARD_UNARY_INT_OP_RET_INT(~);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_bit_not_assign(
    asts::IntegerLiteralAst &lhs)
    -> void {
    // Perform bitwise NOT assignment on an integer literal.
    const auto result_literal = std_intrinsics_bit_not(lhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.val = std::move(result_literal->val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_abs(
    asts::IntegerLiteralAst const &val)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform absolute value on an integer literal.
    SPP_STANDARD_UNARY_SIGNED_INT_OP_RET_INT(cmp_abs);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_eq(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform equality comparison on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_BOOL(==);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_oeq(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform ordered equality comparison on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(==);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ne(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform inequality comparison on two integer literals.
    SPP_STANDARD_BINARY_INT_OP_RET_BOOL(!=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_one(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform ordered inequality comparison on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(!=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_slt(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform signed less-than comparison on two integer literals.
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(<);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ult(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform unsigned less-than comparison on two integer literals.
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(<);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_olt(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform ordered less-than comparison on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(<);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sle(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform signed less-than-or-equal comparison on two integer literals.
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(<=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ule(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform unsigned less-than-or-equal comparison on two integer literals.
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(<=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ole(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform ordered less-than-or-equal comparison on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(<=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sgt(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform signed greater-than comparison on two integer literals.
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(>);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ugt(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform unsigned greater-than comparison on two integer literals.
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(>);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ogt(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform ordered greater-than comparison on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(>);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_sge(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform signed greater-than-or-equal comparison on two integer literals.
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_BOOL(>=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_uge(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform unsigned greater-than-or-equal comparison on two integer literals.
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_BOOL(>=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_oge(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::BooleanLiteralAst> {
    // Perform ordered greater-than-or-equal comparison on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_BOOL(>=);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_smax(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform signed maximum on two integer literals.
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(cmp_smax);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_umax(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform unsigned maximum on two integer literals.
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(cmp_umax);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_smin(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform signed minimum on two integer literals.
    SPP_STANDARD_BINARY_SIGNED_INT_OP_RET_INT(cmp_smin);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_umin(
    asts::IntegerLiteralAst const &lhs,
    asts::IntegerLiteralAst const &rhs)
    -> std::unique_ptr<asts::IntegerLiteralAst> {
    // Perform unsigned minimum on two integer literals.
    SPP_STANDARD_BINARY_UNSIGNED_INT_OP_RET_INT(cmp_umin);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fadd(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform addition on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(+);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fadd_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void {
    // Perform addition assignment on a float literal.
    const auto result_literal = std_intrinsics_fadd(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.int_val = std::move(result_literal->int_val);
    lhs.frac_val = std::move(result_literal->frac_val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fsub(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform subtraction on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(-);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fsub_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void {
    // Perform subtraction assignment on a float literal.
    const auto result_literal = std_intrinsics_fsub(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.int_val = std::move(result_literal->int_val);
    lhs.frac_val = std::move(result_literal->frac_val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fmul(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform multiplication on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(*);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fmul_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void {
    // Perform multiplication assignment on a float literal.
    const auto result_literal = std_intrinsics_fmul(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.int_val = std::move(result_literal->int_val);
    lhs.frac_val = std::move(result_literal->frac_val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fdiv(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform division on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(/);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fdiv_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void {
    // Perform division assignment on a float literal.
    const auto result_literal = std_intrinsics_fdiv(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.int_val = std::move(result_literal->int_val);
    lhs.frac_val = std::move(result_literal->frac_val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_frem(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform remainder on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(cmp_frem);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_frem_assign(
    asts::FloatLiteralAst &lhs,
    asts::FloatLiteralAst const &rhs)
    -> void {
    // Perform remainder assignment on a float literal.
    const auto result_literal = std_intrinsics_frem(lhs, rhs);
    lhs.tok_sign = std::move(result_literal->tok_sign);
    lhs.int_val = std::move(result_literal->int_val);
    lhs.frac_val = std::move(result_literal->frac_val);
    lhs.type = std::move(result_literal->type);
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fneg(
    asts::FloatLiteralAst const &val)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform negation on a float literal.
    SPP_STANDARD_UNARY_FLOAT_OP(-);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fabs(
    asts::FloatLiteralAst const &val)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform absolute value on a float literal.
    SPP_STANDARD_UNARY_FLOAT_OP(cmp_fabs);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fmax(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform maximum on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(cmp_fmax);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fmin(
    asts::FloatLiteralAst const &lhs,
    asts::FloatLiteralAst const &rhs)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform minimum on two float literals.
    SPP_STANDARD_BINARY_FLOAT_OP_RET_FLOAT(cmp_fmin);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ffloor(
    asts::FloatLiteralAst const &val)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform floor operation on a float literal.
    SPP_STANDARD_UNARY_FLOAT_OP(cmp_ffloor);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fceil(
    asts::FloatLiteralAst const &val)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform ceiling operation on a float literal.
    SPP_STANDARD_UNARY_FLOAT_OP(cmp_fceil);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_ftrunc(
    asts::FloatLiteralAst const &val)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform truncation operation on a float literal.
    SPP_STANDARD_UNARY_FLOAT_OP(cmp_ftrunc);
    return nullptr;
}


auto spp::analyse::utils::cmp_utils::std_intrinsics_fround(
    asts::FloatLiteralAst const &val)
    -> std::unique_ptr<asts::FloatLiteralAst> {
    // Perform round operation on a float literal.
    SPP_STANDARD_UNARY_FLOAT_OP(cmp_fround);
    return nullptr;
}

#pragma GCC diagnostic pop
