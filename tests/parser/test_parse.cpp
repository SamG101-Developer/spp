#include "../test_macros.hpp"
import spp.analyse.errors.semantic_error;
import spp.asts.annotation_ast;
import spp.asts.array_literal_ast;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_repeated_element_ast;
import spp.asts.assignment_statement_ast;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.binary_expression_temp_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_destructure_skip_single_argument_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.closure_expression_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.float_literal_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_required_ast;
import spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_type_required_ast;
import spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.gen_expression_ast;
import spp.asts.gen_with_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.is_expression_ast;
import spp.asts.is_expression_temp_ast;
import spp.asts.let_statement_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.let_statement_uninitialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_destructure_array_ast;
import spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.loop_expression_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.object_initializer_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_index_ast;
import spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.primary_expression_ast;
import spp.asts.ret_statement_ast;
import spp.asts.string_literal_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_array_shorthand_ast;
import spp.asts.type_ast;
import spp.asts.type_binary_expression_ast;
import spp.asts.type_binary_expression_temp_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_parenthesised_expression_ast;
import spp.asts.type_postfix_expression_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_tuple_shorthand_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.unary_expression_ast;
import spp.asts.unary_expression_operator_ast;
import spp.asts.unary_expression_operator_async_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.use_statement_ast;
import spp.parse.errors.parser_error;


SPP_TEST_SHOULD_FAIL_SYNTACTIC(
    parse_intentional_error, R"(
    3254gGG
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_class_prototype, R"(
    cls MyClass { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_class_attribute, R"(
    cls MyClass {
        my_attr_1: S32
        my_attr_2: S32
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_sup_prototype_extension, R"(
    sup MyClass ext Copy { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_sup_prototype_functions, R"(
    sup MyClass { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_sup_type_statement, R"(
    sup MyClass {
        type NewType = OldType
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_subroutine_prototype, R"(
    fun my_function() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_coroutine_prototype, R"(
    cor my_coroutine() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_call_argument_keyword, R"(
    fun my_function() -> Void {
        other_function(arg1=other_thing, arg2=2)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_call_argument_positional, R"(
    fun my_function() -> Void {
        other_function(1, 2)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_call_no_arguments, R"(
    fun my_function() -> Void {
        other_function()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_call_arguments, R"(
    fun my_function() -> Void {
        other_function(1, arg=false)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_self_parameter_mov, R"(
    sup MyClass {
        fun my_method(self) -> Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_self_parameter_mut_mov, R"(
    sup MyClass {
        fun my_method(mut self) -> Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_self_parameter_mut, R"(
    sup MyClass {
        fun my_method(&mut self) -> Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_self_parameter_ref, R"(
    sup MyClass {
        fun my_method(&self) -> Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_required_parameters, R"(
    fun my_function(arg1: S32, arg2: S32) -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_optional_parameters, R"(
    fun my_function(arg2: S32 = 0) -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_variadic_parameters, R"(
    fun my_function(arg1: S32, ..arg2: S32) -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_function_parameters, R"(
    fun my_function(arg1: S32, arg2: S32 = 0, ..arg3: S32) -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_argument_type_named, R"(
    fun my_function() -> Void {
        other_function[T=S32, U=Str]()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_argument_type_unnamed, R"(
    fun my_function() -> Void {
        other_function[S32, Str]()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_argument_comp_named, R"(
    fun my_function() -> Void {
        other_function[n=1, m=2]()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_argument_comp_unnamed, R"(
    fun my_function() -> Void {
        other_function[1, 2]()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_arguments, R"(
    fun my_function() -> Void {
        other_function[S32, Str, T=Bool, n=1]()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_type_required, R"(
    fun my_function[T, U]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_type_optional, R"(
    fun my_function[T=S32, U=Str]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_type_variadic, R"(
    fun my_function[T, ..U]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_type, R"(
    fun my_function[T, U=S32, ..V]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_comp_required, R"(
    fun my_function[cmp n: S32, cmp m: S32]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_comp_optional, R"(
    fun my_function[cmp n: S32=1, cmp m: S32=2]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_comp_variadic, R"(
    fun my_function[cmp ..m: S32]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameter_comp, R"(
    fun my_function[cmp n: S32, cmp m: S32=1, cmp ..o: S32]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_parameters, R"(
    fun my_function[cmp n: S32, T, cmp m: S32=1, U=Str, cmp ..o: S32, ..V]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_generic_inline_constraints, R"(
    fun my_function[T: Copy, U: Clone & Copy]() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_annotations_function, R"(
    @annotation1
    fun my_function() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_annotations_class, R"(
    @annotation1
    cls MyClass { }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_a, R"(
    fun my_function() -> Void {
        variable |= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_b, R"(
    fun my_function() -> Void {
        variable ^= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_c, R"(
    fun my_function() -> Void {
        variable &= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_d, R"(
    fun my_function() -> Void {
        variable += other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_e, R"(
    fun my_function() -> Void {
        variable -= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_f, R"(
    fun my_function() -> Void {
        variable *= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_g, R"(
    fun my_function() -> Void {
        variable /= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_h, R"(
    fun my_function() -> Void {
        variable %= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_0_i, R"(
    fun my_function() -> Void {
        variable **= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_1, R"(
    fun my_function() -> Void {
        variable or other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_2, R"(
    fun my_function() -> Void {
        variable and another_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_3, R"(
    fun my_function() -> Void {
        variable is Destructure(a=1, b, ..)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_4_a, R"(
    fun my_function() -> Void {
        variable == other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_4_b, R"(
    fun my_function() -> Void {
        variable != other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_4_c, R"(
    fun my_function() -> Void {
        variable < other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_4_d, R"(
    fun my_function() -> Void {
        variable <= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_4_e, R"(
    fun my_function() -> Void {
        variable > other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_4_f, R"(
    fun my_function() -> Void {
        variable >= other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_5, R"(
    fun my_function() -> Void {
        variable | other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_6, R"(
    fun my_function() -> Void {
        variable ^ other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_7, R"(
    fun my_function() -> Void {
        variable & other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_8_a, R"(
    fun my_function() -> Void {
        variable << other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_8_b, R"(
    fun my_function() -> Void {
        variable >> other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_9_a, R"(
    fun my_function() -> Void {
        variable + other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_9_b, R"(
    fun my_function() -> Void {
        variable - other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_10_a, R"(
    fun my_function() -> Void {
        variable * other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_10_b, R"(
    fun my_function() -> Void {
        variable / other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_10_c, R"(
    fun my_function() -> Void {
        variable % other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_binary_expression_precedence_10_d, R"(
    fun my_function() -> Void {
        variable ** other_variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_unary_expression_async_op, R"(
    fun my_function() -> Void {
        async function()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_unary_expression_deref_op, R"(
    fun my_function() -> Void {
        *borrow
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_postfix_expression_function_call, R"(
    fun my_function() -> Void {
        function()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_postfix_expression_member_access_runtime, R"(
    fun my_function() -> Void {
        variable.field
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_postfix_expression_member_access_runtime_numeric, R"(
    fun my_function() -> Void {
        tuple.0
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_postfix_expression_member_access_static, R"(
    fun my_function() -> Void {
        Type::method()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_postfix_expression_early_return, R"(
    fun my_function() -> Void {
        function()?
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_postfix_expression_not_keyword, R"(
    fun my_function() -> Void {
        variable.not
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_postfix_expression_step_keyword, R"(
    fun my_function() -> Void {
        generator.step
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_parenthesized_expression, R"(
    fun my_function() -> Void {
        (variable)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_self_identifier, R"(
    fun my_function() -> Void {
        self
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_right_fold_expression, R"(
    fun my_function() -> Void {
        tuple + ..
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_left_fold_expression, R"(
    fun my_function() -> Void {
        .. + tuple
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_expression_patterns, R"(
    fun my_function() -> Void {
        case my_tuple of {
            is (1, 2, a) { }
            is (3, b, 4) { }
            is (c, 5, 6) { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_expression_patterns_simple, R"(
    fun my_function() -> Void {
        case my_tuple == (1, 2, 3) {
        }
        else case some_other_expression {
        }
        else {
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_loop_expression_boolean_condition, R"(
    fun my_function() -> Void {
        loop true {
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_loop_expression_iterable_condition, R"(
    fun my_function() -> Void {
        loop i in some_vector {
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_loop_expression_else_block, R"(
    fun my_function() -> Void {
        loop i in some_vector {
        }
        else {
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_gen_no_expression, R"(
    fun my_function() -> Void {
        gen
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_gen_mov_expression, R"(
    fun my_function() -> Void {
        gen variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_gen_ref_expression, R"(
    fun my_function() -> Void {
        gen &variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_gen_mut_expression, R"(
    fun my_function() -> Void {
        gen &mut variable
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_gen_expression_unroll, R"(
    fun my_function() -> Void {
        gen with another_generator
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_ret_statement_no_value, R"(
    fun my_function() -> Void {
        ret
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_ret_statement_value, R"(
    fun my_function() -> Void {
        ret 1
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_exit_statement_no_value, R"(
    fun my_function() -> Void {
        loop true {
            loop true {
                exit exit
            }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_exit_statement_value, R"(
    fun my_function() -> Void {
        loop true {
            loop true {
                exit exit 1
            }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_exit_statement_skip, R"(
    fun my_function() -> Void {
        loop true {
            loop true {
                exit skip
            }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_skip_statement, R"(
    fun my_function() -> Void {
        loop true {
            loop true {
                skip
            }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_inner_scope, R"(
    fun my_function() -> Void {
        {
            inner_function()
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_global_type_statement, R"(
    type MyString = Str

)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_global_constant, R"(
    cmp constant: S32 = 1

)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_global_constant_advanced, R"(
    cmp glob_array_1: Arr[std::bignum::bigint::BigInt, 100_uz] = std::array::Arr[std::bignum::bigint::BigInt, 100_uz]()
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_let_statement_initialized, R"(
    fun my_function() -> Void {
        let a = 1
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_let_statement_uninitialized, R"(
    fun my_function() -> Void {
        let a: S32
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_destructure_with_single_skip, R"(
    fun my_function() -> Void {
        let (a, _, b, _) = tuple
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_destructure_with_multiple_skip, R"(
    fun my_function() -> Void {
        let (a, .., b) = tuple
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_destructure_with_single_identifier_alias, R"(
    fun my_function() -> Void {
        let MyType(attr as a) = object
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_single_identifier, R"(
    fun my_function() -> Void {
        let a = 1
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_destructure_array, R"(
    fun my_function() -> Void {
        let [a, b, c] = array
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_destructure_tuple, R"(
    fun my_function() -> Void {
        let (a, b, c) = tuple
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_destructure_object, R"(
    fun my_function() -> Void {
        let MyType(a, b, c) = object
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_local_variable_destructure_object_attr_binding, R"(
    fun my_function() -> Void {
        let MyType(attr1=Point(x, y), attr2) = object
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_assignment_statement, R"(
    fun my_function() -> Void {
        variable = 1
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_assignment_multiple_statement, R"(
    fun my_function() -> Void {
        a, b, c = 1, 2, 3
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_else_pattern, R"(
    fun my_function() -> Void {
        case value {
        }
        else {
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_else_case_pattern_with_condition, R"(
    fun my_function() -> Void {
        case value == 1 {
        }
        else case value == 2 {
        }
        else case value == 3 {
        }
        else {
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_destructure_array, R"(
    fun my_function() -> Void {
        case value of {
            is [a, b, c] { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_destructure_tuple, R"(
    fun my_function() -> Void {
        case value of {
            is (a, b, c) { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_destructure_object, R"(
    fun my_function() -> Void {
        case value of {
            is MyType(a, b, c) { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_destructure_object_attr_binding, R"(
    fun my_function() -> Void {
        case value of {
            is MyType(attr1=Point(x, y), attr2) { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_destructure_literal, R"(
    fun my_function() -> Void {
        case value of {
            == 1 { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_case_destructure_expression, R"(
    fun my_function() -> Void {
        case array of {
            == some_function_call { }
            == other_function_call { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_pattern_guard, R"(
    fun my_function() -> Void {
        case value of {
            == 1 and some_condition { }
            == 2 and other_condition { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_convention_mutable_borrow, R"(
    fun my_function(a: &mut S32) -> Void {
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_convention_immutable_borrow, R"(
    fun my_function(a: &S32) -> Void {
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_object_initializer_argument_named, R"(
    fun my_function() -> Void {
        MyType(attr1=1, attr2=2)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_object_initializer_argument_unnamed, R"(
    fun my_function() -> Void {
        MyType(attr1, attr2)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_object_initializer_argument_default, R"(
    fun my_function() -> Void {
        MyType(1, ..other)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_object_initializer_arguments, R"(
    fun my_function() -> Void {
        MyType(attr1, attr2=other, ..other)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_type_tuple, R"(
    fun my_function() -> Void {
        let a: (S32, S32)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_type_union, R"(
    fun my_function() -> Void {
        let a: S32 or Str
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_type_single, R"(
    fun my_function() -> Void {
        let a: S32
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_type_with_namespace, R"(
    fun my_function() -> Void {
        let a: std::inner::Str
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_nested_type, R"(
    fun my_function() -> Void {
        let a: std::inner::Str::ValueType::Other
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_type_with_self, R"(
    fun my_function() -> Void {
        let a: Self
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_type_with_self_nested, R"(
    fun my_function() -> Void {
        let a: Self::InnerType
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_float, R"(
    fun my_function() -> Void {
        let a = 1.0
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_integer, R"(
    fun my_function() -> Void {
        let a = 1
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_integer_with_sign, R"(
    fun my_function() -> Void {
        let a = -1
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_integer_with_type, R"(
    fun my_function() -> Void {
        let a = 1_u64
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_integer_base_2, R"(
    fun my_function() -> Void {
        let a = 0b101
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_integer_base_16, R"(
    fun my_function() -> Void {
        let a = 0x1F
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_string, R"(
    fun my_function() -> Void {
        let a = "string"
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_boolean, R"(
    fun my_function() -> Void {
        let a = true
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_literal_tuple_0_items, R"(
    fun my_function() -> Void {
        let a = ()
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_tuple_1_item, R"(
    fun my_function() -> Void {
        let a = (1,)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_tuple_n_items, R"(
    fun my_function() -> Void {
        let a = (1, 2, 3)
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_array_0_items, R"(
    fun my_function() -> Void {
        let a = [S32, 8]
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_array_n_items, R"(
    fun my_function() -> Void {
        let a = [1,2,3]
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_main, R"(
    fun main(args: std::vector::Vec[std::string::Str]) -> std::void::Void {
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_closure_no_params, R"(
    fun my_function() -> Void {
        let my_closure = () { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_closure_with_params, R"(
    fun my_function() -> Void {
        let my_closure = (a: S32, b: S32) { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_closure_with_capture, R"(
    fun my_function() -> Void {
        let my_closure = (caps a, &b, &mut c) { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_closure_with_params_and_capture, R"(
    fun my_function() -> Void {
        let my_closure = (a: S32, b: S32 caps c, &d, &mut e) { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_closure_with_param_optional, R"(
    fun my_function() -> Void {
        let my_closure = (a: S32, b: S32 = 0) { }
    }
)");


SPP_TEST_SHOULD_PASS_SYNTACTIC(
    parse_closure_with_param_variadic, R"(
    fun my_function() -> Void {
        let my_closure = (a: S32, ..b: S32) { a }
    }
)");
