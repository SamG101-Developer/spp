#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/coroutine_prototype_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/literal_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/local_variable_destructure_array_ast.hpp>
#include <spp/asts/local_variable_destructure_object_ast.hpp>
#include <spp/asts/local_variable_destructure_tuple_ast.hpp>
#include <spp/asts/local_variable_destructure_skip_multiple_arguments_ast.hpp>
#include <spp/asts/loop_control_flow_statement_ast.hpp>
#include <spp/asts/iter_pattern_variant_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


// TODO: READ ALL ERRORS: CURRENTLY WRITTEN BY AI


auto spp::analyse::errors::SemanticError::add_header(const std::size_t err_code, std::string &&msg) -> void {
    m_error_info.emplace_back(nullptr, ErrorInformationType::HEADER, std::move(msg), "E" + std::to_string(err_code));
}


auto spp::analyse::errors::SemanticError::add_error(asts::Ast const *ast, std::string &&tag) -> void {
    m_error_info.emplace_back(ast, ErrorInformationType::ERROR, std::move(tag), "");
}


auto spp::analyse::errors::SemanticError::add_context_for_error(asts::Ast const *ast, std::string &&tag) -> void {
    m_error_info.emplace_back(ast, ErrorInformationType::CONTEXT, std::move(tag), "");
}


auto spp::analyse::errors::SemanticError::add_footer(std::string &&note, std::string &&help) -> void {
    m_error_info.emplace_back(nullptr, ErrorInformationType::FOOTER, std::move(note), std::move(help));
}


spp::analyse::errors::SppAnnotationInvalidApplicationError::SppAnnotationInvalidApplicationError(
    asts::AnnotationAst const &annotation,
    asts::Ast const &ctx,
    std::string &&block_list) {
    add_header(
        0, "SPP Annotation Invalid Application Error");
    add_context_for_error(
        &annotation,
        "Annotation defined here");
    add_error(
        &ctx,
        "Invalid " + block_list + "context defined here");
    add_footer(
        "This annotation is not compatible with the current context.",
        "Remove the annotation from here");
}


spp::analyse::errors::SppAnnotationConflictError::SppAnnotationConflictError(
    asts::AnnotationAst const &first_annotation,
    asts::AnnotationAst const &conflicting_annotation,
    asts::Ast const &ctx) {
    add_header(
        1, "SPP Annotation Conflict Error");
    add_context_for_error(
        &first_annotation,
        "First annotation defined here");
    add_context_for_error(
        &conflicting_annotation,
        "Conflicting annotation defined here");
    add_error(
        &ctx,
        "In this context");
    add_footer(
        "These two annotations cannot be applied in the same context.",
        "Remove one of the annotations");
}


spp::analyse::errors::SppAnnotationInvalidError::SppAnnotationInvalidError(
    asts::AnnotationAst const &annotation) {
    add_header(
        2, "SPP Annotation Invalid Error");
    add_error(
        &annotation,
        "Invalid annotation defined here");
    add_footer(
        "This annotation is not recognized.",
        "Remove or correct the annotation");
}


spp::analyse::errors::SppExpressionTypeInvalidError::SppExpressionTypeInvalidError(
    asts::Ast const &expr) {
    add_header(
        3, "SPP Expression Type Invalid Error");
    add_error(
        &expr,
        "Expression with invalid type defined here");
    add_footer(
        "The type of this expression is invalid in the current context.",
        "Ensure the expression has a valid type");
}


spp::analyse::errors::SppTypeMismatchError::SppTypeMismatchError(
    asts::Ast const &lhs,
    asts::TypeAst const &lhs_ty,
    asts::Ast const &rhs,
    asts::TypeAst const &rhs_ty) {
    add_header(
        4, "SPP Type Mismatch Error");
    add_context_for_error(
        &lhs,
        "Left-hand side type: " + static_cast<std::string>(lhs_ty));
    add_error(
        &rhs,
        "Right-hand side type: " + static_cast<std::string>(rhs_ty));
    add_footer(
        "The types of the left-hand side and right-hand side do not match.",
        "Ensure both sides have compatible types");
}


spp::analyse::errors::SppSecondClassBorrowViolationError::SppSecondClassBorrowViolationError(
    asts::Ast const &expr,
    asts::Ast const &conv,
    std::string_view ctx) {
    add_header(
        5, "SPP Second-Class Borrow Violation Error");
    add_context_for_error(
        &expr,
        "Expression defined here");
    add_error(
        &conv,
        "Conversion to second-class type in " + std::string(ctx) + " context defined here");
    add_footer(
        "This expression cannot be used in a " + std::string(ctx) + " context because it is a second-class type.",
        "Use a first-class type or adjust the context");
}


spp::analyse::errors::SppCompileTimeConstantError::SppCompileTimeConstantError(
    asts::ExpressionAst const &expr) {
    add_header(
        6, "SPP Compile-Time Constant Error");
    add_error(
        &expr,
        "Expression defined here");
    add_footer(
        "This expression must be a compile-time constant.",
        "Ensure the expression can be evaluated at compile time");
}


spp::analyse::errors::SppInvalidMutationError::SppInvalidMutationError(
    asts::Ast const &sym,
    asts::Ast const &mutator,
    asts::Ast const &initialization_location) {
    add_header(
        7, "SPP Invalid Mutation Error");
    add_context_for_error(
        &sym,
        "Symbol defined here");
    add_context_for_error(
        &initialization_location,
        "Initialized here");
    add_error(
        &mutator,
        "Invalid mutation attempted here");
    add_footer(
        "This symbol cannot be mutated because it was not declared as mutable.",
        "Declare the symbol as mutable or remove the mutation");
}


spp::analyse::errors::SppUninitializedMemoryUseError::SppUninitializedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::Ast const &init_location,
    asts::Ast const &move_location) {
    add_header(
        8, "SPP Uninitialized Memory Use Error");
    add_context_for_error(
        &ast,
        "Expression using uninitialized memory defined here");
    add_context_for_error(
        &init_location,
        "Memory was expected to be initialized here");
    add_error(
        &move_location,
        "Memory was moved from here");
    add_footer(
        "This expression uses memory that may not have been initialized.",
        "Ensure the memory is properly initialized before use");
}


spp::analyse::errors::SppPartiallyInitializedMemoryUseError::SppPartiallyInitializedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::Ast const &init_location,
    asts::Ast const &partial_move_location) {
    add_header(
        9, "SPP Partially Initialized Memory Use Error");
    add_context_for_error(
        &ast,
        "Expression using partially initialized memory defined here");
    add_context_for_error(
        &init_location,
        "Memory was initialized here");
    add_error(
        &partial_move_location,
        "Part of the memory was moved from here");
    add_footer(
        "This expression uses memory that may only be partially initialized.",
        "Ensure the memory is fully initialized before use");
}


spp::analyse::errors::SppMoveFromBorrowedMemoryError::SppMoveFromBorrowedMemoryError(
    asts::ExpressionAst const &ast,
    asts::Ast const &move_location,
    asts::Ast const &borrow_location) {
    add_header(
        10, "SPP Move From Borrowed Memory Error");
    add_context_for_error(
        &ast,
        "Expression attempting to move from borrowed memory defined here");
    add_context_for_error(
        &borrow_location,
        "Memory was borrowed here");
    add_error(
        &move_location,
        "Move attempted here");
    add_footer(
        "This expression attempts to move from memory that is currently borrowed.",
        "Ensure the memory is not borrowed when moving from it");
}


spp::analyse::errors::SppMoveFromPinnedMemoryError::SppMoveFromPinnedMemoryError(
    asts::ExpressionAst const &ast,
    asts::Ast const &init_location,
    asts::Ast const &move_location,
    asts::Ast const &pin_location) {
    add_header(
        11, "SPP Move From Pinned Memory Error");
    add_context_for_error(
        &ast,
        "Expression attempting to move from pinned memory defined here");
    add_context_for_error(
        &init_location,
        "Memory was initialized here");
    add_context_for_error(
        &pin_location,
        "Memory was pinned here");
    add_error(
        &move_location,
        "Move attempted here");
    add_footer(
        "This expression attempts to move from memory that is currently pinned.",
        "Ensure the memory is unpinned before moving from it");
}


spp::analyse::errors::SppMoveFromPinLinkedMemoryError::SppMoveFromPinLinkedMemoryError(
    asts::ExpressionAst const &ast,
    asts::Ast const &init_location,
    asts::Ast const &move_location,
    asts::Ast const &pin_location,
    asts::Ast const &pin_init_location) {
    add_header(
        12, "SPP Move From Pin-Linked Memory Error");
    add_context_for_error(
        &ast,
        "Expression attempting to move from pin-linked memory defined here");
    add_context_for_error(
        &init_location,
        "Memory was initialized here");
    add_context_for_error(
        &pin_init_location,
        "Pin was initialized here");
    add_context_for_error(
        &pin_location,
        "Memory was pinned here");
    add_error(
        &move_location,
        "Move attempted here");
    add_footer(
        "This expression attempts to move from memory that is linked to a pin.",
        "Ensure the memory is unlinked from the pin before moving from it");
}


spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError::SppInconsistentlyInitializedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::CaseExpressionBranchAst const &branch_1,
    asts::CaseExpressionBranchAst const &branch_2,
    std::string_view what) {
    add_header(
        13, "SPP Inconsistently Initialized Memory Use Error");
    add_context_for_error(
        &ast,
        "Expression using inconsistently initialized memory defined here");
    add_context_for_error(
        &branch_1,
        "In this branch, the memory is " + std::string(what));
    add_error(
        &branch_2,
        "In this branch, the memory is not " + std::string(what));
    add_footer(
        "This expression uses memory that is not consistently " + std::string(what) + " across all branches.",
        "Ensure the memory is consistently " + std::string(what) + " in all branches before use");
}


spp::analyse::errors::SppInconsistentlyPinnedMemoryUseError::SppInconsistentlyPinnedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::CaseExpressionBranchAst const &branch_1,
    asts::CaseExpressionBranchAst const &branch_2) {
    add_header(
        14, "SPP Inconsistently Pinned Memory Use Error");
    add_context_for_error(
        &ast,
        "Expression using inconsistently pinned memory defined here");
    add_context_for_error(
        &branch_1,
        "In this branch, the memory is pinned");
    add_error(
        &branch_2,
        "In this branch, the memory is not pinned");
    add_footer(
        "This expression uses memory that is not consistently pinned across all branches.",
        "Ensure the memory is consistently pinned in all branches before use");
}


spp::analyse::errors::SppCompoundAssignmentTargetError::SppCompoundAssignmentTargetError(
    asts::ExpressionAst const &lhs) {
    add_header(
        15, "SPP Compound Assignment Target Error");
    add_error(
        &lhs,
        "Left-hand side of compound assignment defined here");
    add_footer(
        "The left-hand side of a compound assignment must be a valid assignable target.",
        "Ensure the left-hand side is a variable, field, or indexable element");
}


spp::analyse::errors::SppMemberAccessNonIndexableError::SppMemberAccessNonIndexableError(
    asts::ExpressionAst const &lhs,
    asts::TypeAst const &lhs_type,
    asts::Ast const &access_op) {
    add_header(
        16, "SPP Member Access Non-Indexable Error");
    add_context_for_error(
        &lhs,
        "Left-hand side expression defined here with type: " + static_cast<std::string>(lhs_type));
    add_error(
        &access_op,
        "Member access operator defined here");
    add_footer(
        "The left-hand side expression is not indexable, so member access cannot be performed.",
        "Ensure the left-hand side is an indexable type (e.g., array, tuple, struct)");
}


spp::analyse::errors::SppMemberAccessOutOfBoundsError::SppMemberAccessOutOfBoundsError(
    asts::ExpressionAst const &lhs,
    asts::TypeAst const &lhs_type,
    asts::Ast const &access_op) {
    add_header(
        17, "SPP Member Access Out Of Bounds Error");
    add_context_for_error(
        &lhs,
        "Left-hand side expression defined here with type: " + static_cast<std::string>(lhs_type));
    add_error(
        &access_op,
        "Member access operator defined here");
    add_footer(
        "The member access is out of bounds for the given type.",
        "Ensure the accessed member exists within the bounds of the type");
}


spp::analyse::errors::SppCaseBranchMultipleDestructuresError::SppCaseBranchMultipleDestructuresError(
    asts::CasePatternVariantAst const &first_pattern,
    asts::CasePatternVariantAst const &second_pattern) {
    add_header(
        19, "SPP Case Branch Multiple Destructures Error");
    add_context_for_error(
        &first_pattern,
        "First destructure pattern defined here");
    add_error(
        &second_pattern,
        "Second destructure pattern defined here");
    add_footer(
        "A case branch cannot contain multiple destructure patterns.",
        "Combine the patterns into a single destructure or separate them into different branches");
}


spp::analyse::errors::SppCaseBranchElseNotLastError::SppCaseBranchElseNotLastError(
    asts::CaseExpressionBranchAst const &non_last_else_branch,
    asts::CaseExpressionBranchAst const &last_branch) {
    add_header(
        20, "SPP Case Branch Else Not Last Error");
    add_context_for_error(
        &non_last_else_branch,
        "Non-last 'else' branch defined here");
    add_error(
        &last_branch,
        "Last branch defined here");
    add_footer(
        "The 'else' branch must be the last branch in a case expression.",
        "Move the 'else' branch to be the last branch");
}


spp::analyse::errors::SppInvalidTypeAnnotationError::SppInvalidTypeAnnotationError(
    asts::TypeAst const &type,
    asts::LocalVariableAst const &var) {
    add_header(
        21, "SPP Invalid Type Annotation Error");
    add_context_for_error(
        &var,
        "Variable defined here");
    add_error(
        &type,
        "Invalid type annotation defined here");
    add_footer(
        "The type annotation for this variable is invalid.",
        "Correct or remove the type annotation");
}


spp::analyse::errors::SppMultipleSkipMultiArgumentsError::SppMultipleSkipMultiArgumentsError(
    asts::LocalVariableAst const &var,
    asts::LocalVariableDestructureSkipMultipleArgumentsAst const &first_arg,
    asts::LocalVariableDestructureSkipMultipleArgumentsAst const &second_arg) {
    add_header(
        22, "SPP Multiple Skip Multi-Arguments Error");
    add_context_for_error(
        &var,
        "Variable defined here");
    add_context_for_error(
        &first_arg,
        "First skip multi-argument defined here");
    add_error(
        &second_arg,
        "Second skip multi-argument defined here");
    add_footer(
        "A destructure cannot contain multiple skip multi-arguments.",
        "Remove one of the skip multi-arguments");
}


spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError::SppVariableArrayDestructureArrayTypeMismatchError(
    asts::LocalVariableDestructureArrayAst const &var,
    asts::ExpressionAst const &val,
    asts::TypeAst const &val_type) {
    add_header(
        23, "SPP Variable Array Destructure Array Type Mismatch Error");
    add_context_for_error(
        &var,
        "Array destructure variable defined here");
    add_error(
        &val,
        "Value being destructured defined here with type: " + static_cast<std::string>(val_type));
    add_footer(
        "The type of the value being destructured does not match the expected array type.",
        "Ensure the value is of an array type compatible with the destructure");
}


spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError::SppVariableArrayDestructureArraySizeMismatchError(
    asts::LocalVariableDestructureArrayAst const &var,
    std::size_t var_size,
    asts::ExpressionAst const &val,
    std::size_t val_size) {
    add_header(
        24, "SPP Variable Array Destructure Array Size Mismatch Error");
    add_context_for_error(
        &var,
        "Array destructure variable defined here with size: " + std::to_string(var_size));
    add_error(
        &val,
        "Value being destructured defined here with size: " + std::to_string(val_size));
    add_footer(
        "The size of the value being destructured does not match the expected array size.",
        "Ensure the value has the same size as the destructure variable");
}


spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError::SppVariableTupleDestructureTupleTypeMismatchError(
    asts::LocalVariableDestructureTupleAst const &var,
    asts::ExpressionAst const &val,
    asts::TypeAst const &val_type) {
    add_header(
        25, "SPP Variable Tuple Destructure Tuple Type Mismatch Error");
    add_context_for_error(
        &var,
        "Tuple destructure variable defined here");
    add_error(
        &val,
        "Value being destructured defined here with type: " + static_cast<std::string>(val_type));
    add_footer(
        "The type of the value being destructured does not match the expected tuple type.",
        "Ensure the value is of a tuple type compatible with the destructure");
}


spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError::SppVariableTupleDestructureTupleSizeMismatchError(
    asts::LocalVariableDestructureTupleAst const &var,
    std::size_t var_size,
    asts::ExpressionAst const &val,
    std::size_t val_size) {
    add_header(
        26, "SPP Variable Tuple Destructure Tuple Size Mismatch Error");
    add_context_for_error(
        &var,
        "Tuple destructure variable defined here with size: " + std::to_string(var_size));
    add_error(
        &val,
        "Value being destructured defined here with size: " + std::to_string(val_size));
    add_footer(
        "The size of the value being destructured does not match the expected tuple size.",
        "Ensure the value has the same size as the destructure variable");
}


spp::analyse::errors::SppVariableObjectDestructureWithBoundMultiSkipError::SppVariableObjectDestructureWithBoundMultiSkipError(
    asts::LocalVariableDestructureObjectAst const &var,
    asts::LocalVariableDestructureSkipMultipleArgumentsAst const &multi_skip) {
    add_header(
        27, "SPP Variable Object Destructure With Bound Multi-Skip Error");
    add_context_for_error(
        &var,
        "Object destructure variable defined here");
    add_error(
        &multi_skip,
        "Bound skip multi-argument defined here");
    add_footer(
        "An object destructure cannot contain a bound skip multi-argument.",
        "Remove the bound skip multi-argument from the destructure");
}


spp::analyse::errors::SppExpressionNotBooleanError::SppExpressionNotBooleanError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    std::string_view what) {
    add_header(
        28, "SPP Expression Not Boolean Error");
    add_error(
        &expr,
        "Expression defined here with type: " + static_cast<std::string>(expr_type));
    add_footer(
        "This expression must be of boolean type to be used in a " + std::string(what) + " context.",
        "Ensure the expression evaluates to a boolean value");
}


spp::analyse::errors::SppExpressionNotGeneratorError::SppExpressionNotGeneratorError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    std::string_view what) {
    add_header(
        29, "SPP Expression Not Generator Error");
    add_error(
        &expr,
        "Expression defined here with type: " + static_cast<std::string>(expr_type));
    add_footer(
        "This expression must be of generator type to be used in a " + std::string(what) + " context.",
        "Ensure the expression evaluates to a generator type");
}


spp::analyse::errors::SppExpressionAmbiguousGeneratorError::SppExpressionAmbiguousGeneratorError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    std::string_view what) {
    add_header(
        30, "SPP Expression Ambiguous Generator Error");
    add_error(
        &expr,
        "Expression defined here with type: " + static_cast<std::string>(expr_type));
    add_footer(
        "This expression has an ambiguous generator type in a " + std::string(what) + " context.",
        "Ensure the expression has a clear and unambiguous generator type");
}


spp::analyse::errors::SppIterExpressionPatternTypeDuplicateError::SppIterExpressionPatternTypeDuplicateError(
    asts::IterPatternVariantAst const &first_branch,
    asts::IterPatternVariantAst const &second_branch) {
    add_header(
        34, "SPP Iter Expression Pattern Type Duplicate Error");
    add_context_for_error(
        &first_branch,
        "First pattern branch defined here");
    add_error(
        &second_branch,
        "Second pattern branch defined here");
    add_footer(
        "These two pattern branches have the same type, which is not allowed.",
        "Ensure each pattern branch has a unique type");
}


spp::analyse::errors::SppIterExpressionPatternIncompatibleError::SppIterExpressionPatternIncompatibleError(
    asts::ExpressionAst const &cond,
    asts::TypeAst const &cond_type,
    asts::IterPatternVariantAst const &pattern,
    asts::TypeAst const &required_generator_type) {
    add_header(
        31, "SPP Iter Expression Pattern Incompatible Error");
    add_context_for_error(
        &cond,
        "Condition expression defined here with type: " + static_cast<std::string>(cond_type));
    add_error(
        &pattern,
        "Pattern defined here");
    add_footer(
        "The pattern is incompatible with the generator type required for the condition expression.",
        "Ensure the pattern matches the required generator type: " + static_cast<std::string>(required_generator_type));
}


spp::analyse::errors::SppIterExpressionPatternMissingError::SppIterExpressionPatternMissingError(
    asts::ExpressionAst const &cond,
    asts::TypeAst const &cond_type) {
    add_header(
        32, "SPP Iter Expression Pattern Missing Error");
    add_error(
        &cond,
        "Condition expression defined here with type: " + static_cast<std::string>(cond_type));
    add_footer(
        "A pattern is required for the generator type of the condition expression.",
        "Provide a suitable pattern for the generator type");
}


spp::analyse::errors::SppCaseBranchMissingElseError::SppCaseBranchMissingElseError(
    asts::CaseExpressionAst const &case_expr,
    asts::CaseExpressionBranchAst const &last_branch) {
    add_header(
        33, "SPP Case Branch Missing Else Error");
    add_context_for_error(
        &case_expr,
        "Case expression defined here");
    add_error(
        &last_branch,
        "Last branch defined here");
    add_footer(
        "A case expression must have an 'else' branch to handle all possible cases.",
        "Add an 'else' branch to the case expression");
}


spp::analyse::errors::SppIdentifierDuplicateError::SppIdentifierDuplicateError(
    asts::Ast const &first_identifier,
    asts::Ast const &duplicate_identifier,
    std::string_view what) {
    add_header(
        18, "SPP Identifier Duplicate Error");
    add_context_for_error(
        &first_identifier,
        "First " + std::string(what) + " identifier defined here");
    add_error(
        &duplicate_identifier,
        "Duplicate " + std::string(what) + " identifier defined here");
    add_footer(
        "This " + std::string(what) + " identifier has already been defined in the current scope.",
        "Rename or remove the duplicate identifier");
}


spp::analyse::errors::SppRecursiveTypeError::SppRecursiveTypeError(
    asts::ClassPrototypeAst const &type,
    asts::TypeAst const &recursion) {
    add_header(
        35, "SPP Recursive Type Error");
    add_context_for_error(
        &type,
        "Type defined here");
    add_error(
        &recursion,
        "Recursive reference defined here");
    add_footer(
        "This type is recursively defined in a way that is not allowed.",
        "Modify the type definition to eliminate illegal recursion");
}


spp::analyse::errors::SppCoroutineInvalidReturnTypeError::SppCoroutineInvalidReturnTypeError(
    asts::CoroutinePrototypeAst const &proto,
    asts::TypeAst const &return_type) {
    add_header(
        36, "SPP Coroutine Invalid Return Type Error");
    add_context_for_error(
        &proto,
        "Coroutine prototype defined here");
    add_error(
        &return_type,
        "Invalid return type defined here");
    add_footer(
        "The return type of a coroutine must be a generator type.",
        "Change the return type to 'Gen/GenOpt/GenRes/GenOnce'");
}


spp::analyse::errors::SppFloatOutOfBoundsError::SppFloatOutOfBoundsError(
    asts::LiteralAst const &literal,
    boost::multiprecision::cpp_dec_float_100 value,
    boost::multiprecision::cpp_dec_float_100 lower,
    boost::multiprecision::cpp_dec_float_100 upper,
    std::string_view what) {
    add_header(
        37, "SPP Float Out Of Bounds Error");
    add_error(
        &literal,
        "Float literal defined here with value: " + value.str());
    add_footer(
        "The value of this float literal is out of bounds for " + std::string(what) + " type.",
        "Ensure the value is within the range: [" + lower.str() + ", " + upper.str() + "]");
}


spp::analyse::errors::SppIntegerOutOfBoundsError::SppIntegerOutOfBoundsError(
    asts::LiteralAst const &literal,
    boost::multiprecision::cpp_int value,
    boost::multiprecision::cpp_int lower,
    boost::multiprecision::cpp_int upper,
    std::string_view what) {
    add_header(
        38, "SPP Integer Out Of Bounds Error");
    add_error(
        &literal,
        "Integer literal defined here with value: " + value.str());
    add_footer(
        "The value of this integer literal is out of bounds for " + std::string(what) + " type.",
        "Ensure the value is within the range: [" + lower.str() + ", " + upper.str() + "]");
}


spp::analyse::errors::SppOrderInvalidError::SppOrderInvalidError(
    std::string_view first_what,
    asts::Ast const &first,
    std::string_view second_what,
    asts::Ast const &second) {
    add_header(
        39, "SPP Order Invalid Error");
    add_context_for_error(
        &first,
        "First " + std::string(first_what) + " defined here");
    add_error(
        &second,
        "Second " + std::string(second_what) + " defined here");
    add_footer(
        "The order of these two elements is invalid.",
        "Ensure the order of the elements is correct");
}


spp::analyse::errors::SppExpansionOfNonTupleError::SppExpansionOfNonTupleError(
    asts::Ast const &ast,
    asts::TypeAst const &type) {
    add_header(
        40, "SPP Expansion Of Non-Tuple Error");
    add_error(
        &ast,
        "Expression defined here with type: " + static_cast<std::string>(type));
    add_footer(
        "This expression is being expanded, but it is not of tuple type.",
        "Ensure the expression is of tuple type before expanding");
}


spp::analyse::errors::SppMemoryOverlapUsageError::SppMemoryOverlapUsageError(
    asts::Ast const &ast,
    asts::Ast const &overlap_ast) {
    add_header(
        41, "SPP Memory Overlap Usage Error");
    add_context_for_error(
        &ast,
        "Expression defined here");
    add_error(
        &overlap_ast,
        "Overlapping memory region defined here");
    add_footer(
        "This expression uses memory that overlaps with another memory region in an invalid way.",
        "Ensure the memory regions do not overlap or are used safely");
}


spp::analyse::errors::SppMultipleSelfParametersError::SppMultipleSelfParametersError(
    asts::FunctionParameterSelfAst const &first_self,
    asts::FunctionParameterSelfAst const &second_self) {
    add_header(
        42, "SPP Multiple Self Parameters Error");
    add_context_for_error(
        &first_self,
        "First 'self' parameter defined here");
    add_error(
        &second_self,
        "Second 'self' parameter defined here");
    add_footer(
        "A function cannot have multiple 'self' parameters.",
        "Remove one of the 'self' parameters");
}


spp::analyse::errors::SppSelfParamInFreeFunctionError::SppSelfParamInFreeFunctionError(
    asts::FunctionPrototypeAst const &function_proto,
    asts::FunctionParameterSelfAst const &self_param) {
    add_header(
        43, "SPP Self Parameter In Free Function Error");
    add_context_for_error(
        &function_proto,
        "Function prototype defined here");
    add_error(
        &self_param,
        "'self' parameter defined here");
    add_footer(
        "A free function cannot have a 'self' parameter.",
        "Remove the 'self' parameter from the function");
}


spp::analyse::errors::SppFunctionPrototypeConflictError::SppFunctionPrototypeConflictError(
    asts::FunctionPrototypeAst const &first_proto,
    asts::FunctionPrototypeAst const &second_proto) {
    add_header(
        44, "SPP Function Prototype Conflict Error");
    add_context_for_error(
        &first_proto,
        "First function prototype defined here");
    add_error(
        &second_proto,
        "Conflicting function prototype defined here");
    add_footer(
        "These two function prototypes conflict with each other.",
        "Rename or modify one of the function prototypes to resolve the conflict");
}


spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError::SppFunctionSubroutineContainsGenExpressionError(
    asts::TokenAst const &fun_tag,
    asts::TokenAst const &gen_expr) {
    add_header(
        45, "SPP Function Subroutine Contains Generator Expression Error");
    add_context_for_error(
        &fun_tag,
        "Function or subroutine defined here");
    add_error(
        &gen_expr,
        "Generator expression defined here");
    add_footer(
        "A function or subroutine cannot contain a generator expression.",
        "Remove the generator expression or change the function to a coroutine");
}


spp::analyse::errors::SppYieldedTypeMismatchError::SppYieldedTypeMismatchError(
    asts::Ast const &lhs,
    asts::TypeAst const &lhs_ty,
    asts::Ast const &rhs,
    asts::TypeAst const &rhs_ty,
    bool is_optional,
    bool is_fallible,
    asts::TypeAst const &error_type) {
    add_header(
        46, "SPP Yielded Type Mismatch Error");
    add_context_for_error(
        &lhs,
        "Yielded type: " + static_cast<std::string>(lhs_ty) + (is_optional ? " (optional)" : "") + (is_fallible ? " (fallible)" : ""));
    add_error(
        &rhs,
        "Expected type: " + static_cast<std::string>(rhs_ty) + (is_fallible ? " or error type: " + static_cast<std::string>(error_type) : ""));
    add_footer(
        "The type of the yielded value does not match the expected type.",
        "Ensure the yielded value matches the expected type");
}


spp::analyse::errors::SppIdentifierUnknownError::SppIdentifierUnknownError(
    asts::Ast const &name,
    std::string_view what,
    std::optional<std::string> closest) {
    add_header(
        34, "SPP Identifier Unknown Error");
    add_error(
        &name,
        "Unknown " + std::string(what) + " identifier defined here" + (closest ? " (did you mean '" + *closest + "'?)" : ""));
    add_footer(
        "This " + std::string(what) + " identifier is not defined in the current scope.",
        "Define the identifier or correct its name");
}


spp::analyse::errors::SppUnreachableCodeError::SppUnreachableCodeError(
    asts::Ast const &member,
    asts::Ast const &next_member) {
    add_header(
        47, "SPP Unreachable Code Error");
    add_context_for_error(
        &member,
        "Code defined here");
    add_error(
        &next_member,
        "Unreachable code defined here");
    add_footer(
        "This code is unreachable due to preceding control flow statements.",
        "Remove or modify the unreachable code");
}


spp::analyse::errors::SppLoopTooManyControlFlowStatementsError::SppLoopTooManyControlFlowStatementsError(
    asts::TokenAst const &tok_loop,
    asts::LoopControlFlowStatementAst const &stmt,
    std::size_t num_controls,
    std::size_t loop_depth) {
    add_header(
        48, "SPP Loop Too Many Control Flow Statements Error");
    add_context_for_error(
        &tok_loop,
        "Loop defined here with depth: " + std::to_string(loop_depth));
    add_error(
        &stmt,
        "Control flow statement defined here (" + std::to_string(num_controls) + " total)");
    add_footer(
        "This loop contains too many control flow statements (break/continue) for its depth.",
        "Reduce the number of control flow statements or increase the loop depth");
}
