module;
#include <spp/macros.hpp>

module spp.analyse.errors.semantic_error;
import spp.asts.ast;
import spp.asts.annotation_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.class_prototype_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.expression_ast;
import spp.asts.function_prototype_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_destructure_array_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.module_prototype_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_statement_ast;

// TODO: READ ALL ERRORS: CURRENTLY WRITTEN BY AI

SPP_MOD_BEGIN
auto spp::analyse::errors::SemanticError::AddHeaders(
    const std::size_t err_code,
    Str &&msg)
    -> void {
    // Add a header to the error.
    m_error_info.EmplaceBack(nullptr, ErrorInformationType::HEADER, std::move(msg), "E" + std::to_string(err_code));
}

auto spp::analyse::errors::SemanticError::AddErr(
    asts::Ast const *ast,
    Str &&tag)
    -> void {
    // Add an error information entry for the given AST and tag.
    m_error_info.EmplaceBack(ast, ErrorInformationType::ERROR, std::move(tag), "");
}

auto spp::analyse::errors::SemanticError::AddCtxForErr(
    asts::Ast const *ast,
    Str &&tag)
    -> void {
    // Add a context information entry for the given AST and tag.
    m_error_info.EmplaceBack(ast, ErrorInformationType::CONTEXT, std::move(tag), "");
}

auto spp::analyse::errors::SemanticError::AddFooter(
    Str &&note,
    Str &&help)
    -> void {
    // Add a footer to the error with the given note and help message.
    m_error_info.EmplaceBack(nullptr, ErrorInformationType::FOOTER, std::move(note), std::move(help));
}

auto spp::analyse::errors::SemanticError::Clone() const
    -> Unique<SemanticError> {
    // Use the copy constructor to clone the error.
    return MakeUnique<SemanticError>(*this);
}

spp::analyse::errors::SppExpressionTypeInvalidError::SppExpressionTypeInvalidError(
    asts::Ast const &expr) {
    AddHeaders(
        3, "SPP Expression Type Invalid Error");
    AddErr(
        &expr,
        "Expression with invalid type defined here");
    AddFooter(
        "The type of this expression is invalid in the current context.",
        "Ensure the expression has a valid type");
}

spp::analyse::errors::SppTypeMismatchError::SppTypeMismatchError(
    asts::Ast const &lhs,
    asts::TypeAst const &lhs_ty,
    asts::Ast const &rhs,
    asts::TypeAst const &rhs_ty) {
    AddHeaders(
        4, "SPP Type Mismatch Error");
    AddCtxForErr(
        &lhs,
        "Left-hand side type: " + lhs_ty.ToString());
    AddErr(
        &rhs,
        "Right-hand side type: " + rhs_ty.ToString());
    AddFooter(
        "The types of the left-hand side and right-hand side do not match.",
        "Ensure both sides have compatible types");
}

spp::analyse::errors::SppSecondClassBorrowViolationError::SppSecondClassBorrowViolationError(
    asts::Ast const &expr,
    asts::Ast const &type,
    const StrView ctx) {
    AddHeaders(
        5, "SPP Second-Class Borrow Violation Error");
    AddCtxForErr(
        &expr,
        "Expression defined here");
    AddErr(
        &type,
        "Conversion to second-class type in " + Str(ctx) + " context defined here");
    AddFooter(
        "This expression cannot be used in a " + Str(ctx) + " context because it is a second-class type.",
        "Use a first-class type or adjust the context");
}

spp::analyse::errors::SppCompileTimeConstantError::SppCompileTimeConstantError(
    asts::ExpressionAst const &expr) {
    AddHeaders(
        6, "SPP Compile-Time Constant Error");
    AddErr(
        &expr,
        "Expression defined here");
    AddFooter(
        "This expression must be a compile-time constant.",
        "Ensure the expression can be evaluated at compile time");
}

spp::analyse::errors::SppInvalidMutationError::SppInvalidMutationError(
    asts::Ast const &sym,
    asts::Ast const &mutator,
    asts::Ast const &initialization_location) {
    AddHeaders(
        7, "SPP Invalid Mutation Error");
    AddCtxForErr(
        &sym,
        "Symbol defined here");
    AddCtxForErr(
        &initialization_location,
        "Initialized here");
    AddErr(
        &mutator,
        "Invalid mutation attempted here");
    AddFooter(
        "The symbol " + sym.ToString() + " cannot be mutated because it was not declared as mutable.",
        "Declare the symbol as mutable or remove the mutation");
}

spp::analyse::errors::SppUninitializedMemoryUseError::SppUninitializedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::Ast const &init_location,
    asts::Ast const &move_location) {
    AddHeaders(
        8, "SPP Uninitialized Memory Use Error");
    AddCtxForErr(
        &init_location,
        "Memory was initialized here");
    AddCtxForErr(
        &move_location,
        "Memory was moved here");
    AddErr(
        &ast,
        "Expression using uninitialized memory");
    AddFooter(
        "This expression uses memory that may not have been initialized.",
        "Ensure the memory is properly initialized before use");
}

spp::analyse::errors::SppPartiallyInitializedMemoryUseError::SppPartiallyInitializedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::Ast const &init_location,
    asts::Ast const &partial_move_location) {
    AddHeaders(
        9, "SPP Partially Initialized Memory Use Error");
    AddCtxForErr(
        &ast,
        "Expression using partially initialized memory defined here");
    AddCtxForErr(
        &init_location,
        "Memory was initialized here");
    AddErr(
        &partial_move_location,
        "Part of the memory was moved from here");
    AddFooter(
        "This expression uses memory that may only be partially initialized.",
        "Ensure the memory is fully initialized before use");
}

spp::analyse::errors::SppMoveFromBorrowedMemoryError::SppMoveFromBorrowedMemoryError(
    asts::ExpressionAst const &ast,
    asts::Ast const &,
    asts::Ast const &borrow_location) {
    AddHeaders(
        10, "SPP Move From Borrowed Memory Error");
    AddCtxForErr(
        &borrow_location,
        "Memory was borrowed here");
    AddErr(
        &ast,
        "Expression attempting to move from borrowed memory");
    AddFooter(
        "This expression attempts to move from memory that is currently borrowed.",
        "Ensure the memory is not borrowed when moving from it");
}

spp::analyse::errors::SppMoveFromPinnedMemoryError::SppMoveFromPinnedMemoryError(
    asts::ExpressionAst const &ast,
    asts::Ast const &init_location,
    asts::Ast const &move_location,
    asts::Ast const &pin_location) {
    AddHeaders(
        11, "SPP Move From Pinned Memory Error");
    AddCtxForErr(
        &ast,
        "Expression attempting to move from pinned memory");
    AddCtxForErr(
        &init_location,
        "Memory was initialized here");
    AddCtxForErr(
        &pin_location,
        "Memory was pinned here");
    AddErr(
        &move_location,
        "Move attempted here");
    AddFooter(
        "This expression attempts to move from memory that is currently pinned.",
        "Ensure the memory is unpinned before moving from it");
}

spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError::SppInconsistentlyInitializedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::Ast const &branch_1,
    asts::Ast const &branch_2,
    const StrView what) {
    AddHeaders(
        13, "SPP Inconsistently Initialized Memory Use Error");
    AddCtxForErr(
        &ast,
        "Expression using inconsistently initialized memory defined here");
    AddCtxForErr(
        &branch_1,
        "In this branch, the memory is " + Str(what));
    AddErr(
        &branch_2,
        "In this branch, the memory is not " + Str(what));
    AddFooter(
        "This expression uses memory that is not consistently " + Str(what) + " across all branches.",
        "Ensure the memory is consistently " + Str(what) + " in all branches before use");
}

spp::analyse::errors::SppInconsistentlyPinnedMemoryUseError::SppInconsistentlyPinnedMemoryUseError(
    asts::ExpressionAst const &ast,
    asts::Ast const &branch_1,
    asts::Ast const &branch_2) {
    AddHeaders(
        14, "SPP Inconsistently Pinned Memory Use Error");
    AddCtxForErr(
        &ast,
        "Expression using inconsistently pinned memory defined here");
    AddCtxForErr(
        &branch_1,
        "In this branch, the memory is pinned");
    AddErr(
        &branch_2,
        "In this branch, the memory is not pinned");
    AddFooter(
        "This expression uses memory that is not consistently pinned across all branches.",
        "Ensure the memory is consistently pinned in all branches before use");
}

spp::analyse::errors::SppMemberAccessNonIndexableError::SppMemberAccessNonIndexableError(
    asts::ExpressionAst const &lhs,
    asts::TypeAst const &lhs_type,
    asts::Ast const &access_op) {
    AddHeaders(
        16, "SPP Member Access Non-Indexable Error");
    AddCtxForErr(
        &lhs,
        "Left-hand side expression defined here with type: " + lhs_type.ToString());
    AddErr(
        &access_op,
        "Member access operator defined here");
    AddFooter(
        "The left-hand side expression is not indexable, so member access cannot be performed.",
        "Ensure the left-hand side is an indexable type (e.g., array, tuple, struct)");
}

spp::analyse::errors::SppMemberAccessOutOfBoundsError::SppMemberAccessOutOfBoundsError(
    asts::ExpressionAst const &lhs,
    asts::TypeAst const &lhs_type,
    asts::Ast const &access_op) {
    AddHeaders(
        17, "SPP Member Access Out Of Bounds Error");
    AddCtxForErr(
        &lhs,
        "Left-hand side expression defined here with type: " + lhs_type.ToString());
    AddErr(
        &access_op,
        "Member access operator defined here");
    AddFooter(
        "The member access is out of bounds for the given type.",
        "Ensure the accessed member exists within the bounds of the type");
}

spp::analyse::errors::SppCaseBranchMultipleDestructuresError::SppCaseBranchMultipleDestructuresError(
    asts::CasePatternVariantAst const &first_pattern,
    asts::CasePatternVariantAst const &second_pattern) {
    AddHeaders(
        19, "SPP Case Branch Multiple Destructures Error");
    AddCtxForErr(
        &first_pattern,
        "First destructure pattern defined here");
    AddErr(
        &second_pattern,
        "Second destructure pattern defined here");
    AddFooter(
        "A case branch cannot contain multiple destructure patterns.",
        "Combine the patterns into a single destructure or separate them into different branches");
}

spp::analyse::errors::SppCaseBranchElseNotLastError::SppCaseBranchElseNotLastError(
    asts::CaseExpressionBranchAst const &non_last_else_branch,
    asts::CaseExpressionBranchAst const &last_branch) {
    AddHeaders(
        20, "SPP Case Branch Else Not Last Error");
    AddCtxForErr(
        &non_last_else_branch,
        "Non-last 'else' branch defined here");
    AddErr(
        &last_branch,
        "Last branch defined here");
    AddFooter(
        "The 'else' branch must be the last branch in a case expression.",
        "Move the 'else' branch to be the last branch");
}

spp::analyse::errors::SppCaseBranchMissingElseError::SppCaseBranchMissingElseError(
    asts::CaseExpressionAst const &case_expr,
    asts::CaseExpressionBranchAst const &last_branch) {
    AddHeaders(
        33, "SPP Case Branch Missing Else Error");
    AddCtxForErr(
        &case_expr,
        "Case expression defined here");
    AddErr(
        &last_branch,
        "Last branch defined here");
    AddFooter(
        "A case expression must have an 'else' branch to handle all possible cases.",
        "Add an 'else' branch to the case expression");
}

spp::analyse::errors::SppIdentifierDuplicateError::SppIdentifierDuplicateError(
    asts::Ast const &first_identifier,
    asts::Ast const &duplicate_identifier,
    const StrView what) {
    AddHeaders(
        18, "SPP Identifier Duplicate Error");
    AddCtxForErr(
        &first_identifier,
        "First " + Str(what) + " '" + first_identifier.ToString() + "' defined here");
    AddErr(
        &duplicate_identifier,
        "Duplicate " + Str(what) + " '" + duplicate_identifier.ToString() + "' defined here");
    AddFooter(
        "This " + Str(what) + " identifier has already been defined in the current scope.",
        "Rename or remove the duplicate identifier");
}

spp::analyse::errors::SppRecursiveTypeError::SppRecursiveTypeError(
    asts::ClassPrototypeAst const &type,
    asts::TypeAst const &recursion) {
    AddHeaders(
        35, "SPP Recursive Type Error");
    AddCtxForErr(
        &type,
        "Type defined here");
    AddErr(
        &recursion,
        "Recursive reference defined here");
    AddFooter(
        "This type is recursively defined in a way that is not allowed.",
        "Modify the type definition to eliminate illegal recursion");
}

spp::analyse::errors::SppCoroutineInvalidReturnTypeError::SppCoroutineInvalidReturnTypeError(
    asts::CoroutinePrototypeAst const &proto,
    asts::TypeAst const &return_type) {
    AddHeaders(
        36, "SPP Coroutine Invalid Return Type Error");
    AddCtxForErr(
        &proto,
        "Coroutine prototype defined here");
    AddErr(
        &return_type,
        "Invalid return type defined here");
    AddFooter(
        "The return type of a coroutine must be a generator type.",
        "Change the return type to 'Gen/GenOpt/GenRes/GenOnce'");
}

spp::analyse::errors::SppFloatOutOfBoundsError::SppFloatOutOfBoundsError(
    asts::LiteralAst const &literal,
    boost::BigDec const &value,
    boost::BigDec const &lower,
    boost::BigDec const &upper,
    const StrView what) {
    AddHeaders(
        37, "SPP Float Out Of Bounds Error");
    AddErr(
        &literal,
        "Float literal defined here with value: " + value.str());
    AddFooter(
        "The value of this float literal is out of bounds for " + Str(what) + " type.",
        "Ensure the value is within the range: [" + lower.str() + ", " + upper.str() + "]");
}

spp::analyse::errors::SppIntegerOutOfBoundsError::SppIntegerOutOfBoundsError(
    asts::LiteralAst const &literal,
    boost::BigInt const &value,
    boost::BigInt const &lower,
    boost::BigInt const &upper,
    const StrView what) {
    AddHeaders(
        38, "SPP Integer Out Of Bounds Error");
    AddErr(
        &literal,
        "Integer literal defined here with value: " + value.str());
    AddFooter(
        "The value of this integer literal is out of bounds for " + Str(what) + " type.",
        "Ensure the value is within the range: [" + lower.str() + ", " + upper.str() + "]");
}

spp::analyse::errors::SppOrderInvalidError::SppOrderInvalidError(
    const StrView first_what,
    asts::Ast const &first,
    const StrView second_what,
    asts::Ast const &second) {
    AddHeaders(
        39, "SPP Order Invalid Error");
    AddCtxForErr(
        &first,
        "First " + Str(first_what) + " defined here");
    AddErr(
        &second,
        "Second " + Str(second_what) + " defined here");
    AddFooter(
        "The order of these two elements is invalid.",
        "Ensure the order of the elements is correct");
}

spp::analyse::errors::SppExpansionOfNonTupleError::SppExpansionOfNonTupleError(
    asts::Ast const &ast,
    asts::TypeAst const &type) {
    AddHeaders(
        40, "SPP Expansion Of Non-Tuple Error");
    AddErr(
        &ast,
        "Expression defined here with type: " + type.ToString());
    AddFooter(
        "This expression is being expanded, but it is not of tuple type.",
        "Ensure the expression is of tuple type before expanding");
}

spp::analyse::errors::SppMemoryOverlapUsageError::SppMemoryOverlapUsageError(
    asts::Ast const &ast,
    asts::Ast const &overlap_ast) {
    AddHeaders(
        41, "SPP Memory Overlap Usage Error");
    AddCtxForErr(
        &ast,
        "Expression defined here");
    AddErr(
        &overlap_ast,
        "Overlapping memory region defined here");
    AddFooter(
        "This expression uses memory that overlaps with another memory region in an invalid way.",
        "Ensure the memory regions do not overlap or are used safely");
}

spp::analyse::errors::SppMultipleSelfParametersError::SppMultipleSelfParametersError(
    asts::FunctionParameterSelfAst const &first_self,
    asts::FunctionParameterSelfAst const &second_self) {
    AddHeaders(
        42, "SPP Multiple Self Parameters Error");
    AddCtxForErr(
        &first_self,
        "First 'self' parameter defined here");
    AddErr(
        &second_self,
        "Second 'self' parameter defined here");
    AddFooter(
        "A function cannot have multiple 'self' parameters.",
        "Remove one of the 'self' parameters");
}

spp::analyse::errors::SppMultipleVariadicParametersError::SppMultipleVariadicParametersError(
    asts::FunctionParameterVariadicAst const &first_variadic,
    asts::FunctionParameterVariadicAst const &second_variadic) {
    AddHeaders(
        43, "SPP Multiple Variadic Parameters Error");
    AddCtxForErr(
        &first_variadic,
        "First variadic parameter defined here");
    AddErr(
        &second_variadic,
        "Second variadic parameter defined here");
    AddFooter(
        "A function cannot have multiple variadic parameters.",
        "Remove one of the variadic parameters");
}

spp::analyse::errors::SppSelfParamInFreeFunctionError::SppSelfParamInFreeFunctionError(
    asts::FunctionPrototypeAst const &function_proto,
    asts::FunctionParameterSelfAst const &self_param) {
    AddHeaders(
        43, "SPP Self Parameter In Free Function Error");
    AddCtxForErr(
        &function_proto,
        "Function prototype defined here");
    AddErr(
        &self_param,
        "'self' parameter defined here");
    AddFooter(
        "A free function cannot have a 'self' parameter.",
        "Remove the 'self' parameter from the function");
}

spp::analyse::errors::SppFunctionPrototypeConflictError::SppFunctionPrototypeConflictError(
    asts::FunctionPrototypeAst const &first_proto,
    asts::FunctionPrototypeAst const &second_proto) {
    AddHeaders(
        44, "SPP Function Prototype Conflict Error");
    AddCtxForErr(
        &first_proto,
        "First function prototype defined here");
    AddErr(
        &second_proto,
        "Conflicting function prototype defined here");
    AddFooter(
        "These two function prototypes conflict with each other.",
        "Rename or modify one of the function prototypes to resolve the conflict");
}

spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError::SppFunctionSubroutineContainsGenExpressionError(
    asts::TokenAst const &fun_tag,
    asts::TokenAst const &gen_expr) {
    AddHeaders(
        45, "SPP Function Subroutine Contains Generator Expression Error");
    AddCtxForErr(
        &fun_tag,
        "Function or subroutine defined here");
    AddErr(
        &gen_expr,
        "Generator expression defined here");
    AddFooter(
        "A function or subroutine cannot contain a generator expression.",
        "Remove the generator expression or change the function to a coroutine");
}

spp::analyse::errors::SppYieldedTypeMismatchError::SppYieldedTypeMismatchError(
    asts::Ast const &lhs,
    asts::TypeAst const &lhs_ty,
    asts::Ast const &rhs,
    asts::TypeAst const &rhs_ty) {
    AddHeaders(
        46, "SPP Yielded Type Mismatch Error");
    AddCtxForErr(
        &lhs,
        "Yielded type: " + lhs_ty.ToString());
    AddErr(
        &rhs,
        "Expected type: " + rhs_ty.ToString());
    AddFooter(
        "The type of the yielded value does not match the expected type.",
        "Ensure the yielded value matches the expected type");
}

spp::analyse::errors::SppIdentifierUnknownError::SppIdentifierUnknownError(
    asts::Ast const &name,
    const StrView what,
    std::optional<Str> const &closest) {
    AddHeaders(
        34, "SPP Identifier Unknown Error");
    AddErr(
        &name,
        "Unknown " + Str(what) + " '" + name.ToString() + "' defined here" + (closest ? " (did you mean '" + *closest + "'?)" : ""));
    AddFooter(
        "The " + Str(what) + " '" + name.ToString() + "' is not defined in the current scope.",
        "Define the identifier or correct its name");
}

spp::analyse::errors::SppUnreachableCodeError::SppUnreachableCodeError(
    asts::Ast const &member,
    asts::Ast const &next_member) {
    AddHeaders(
        47, "SPP Unreachable Code Error");
    AddCtxForErr(
        &member,
        "Code defined here");
    AddErr(
        &next_member,
        "Unreachable code defined here");
    AddFooter(
        "This code is unreachable due to preceding control flow statements.",
        "Remove or modify the unreachable code");
}

spp::analyse::errors::SppInvalidTypeAnnotationError::SppInvalidTypeAnnotationError(
    asts::TypeAst const &type,
    asts::LocalVariableAst const &var) {
    AddHeaders(
        21, "SPP Invalid Type Annotation Error");
    AddCtxForErr(
        &var,
        "Variable defined here");
    AddErr(
        &type,
        "Invalid type annotation defined here");
    AddFooter(
        "The type annotation for this variable is invalid.",
        "Correct or remove the type annotation");
}

spp::analyse::errors::SppMultipleSkipMultiArgumentsError::SppMultipleSkipMultiArgumentsError(
    asts::LocalVariableAst const &var,
    asts::LocalVariableDestructureSkipMultipleArgumentsAst const &first_arg,
    asts::LocalVariableDestructureSkipMultipleArgumentsAst const &second_arg) {
    AddHeaders(
        22, "SPP Multiple Skip Multi-Arguments Error");
    AddCtxForErr(
        &var,
        "Variable defined here");
    AddCtxForErr(
        &first_arg,
        "First skip multi-argument defined here");
    AddErr(
        &second_arg,
        "Second skip multi-argument defined here");
    AddFooter(
        "A destructure cannot contain multiple skip multi-arguments.",
        "Remove one of the skip multi-arguments");
}

spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError::SppVariableArrayDestructureArrayTypeMismatchError(
    asts::LocalVariableDestructureArrayAst const &var,
    asts::ExpressionAst const &val,
    asts::TypeAst const &val_type) {
    AddHeaders(
        23, "SPP Variable Array Destructure Array Type Mismatch Error");
    AddCtxForErr(
        &var,
        "Array destructure variable defined here");
    AddErr(
        &val,
        "Value being destructured defined here with type: " + val_type.ToString());
    AddFooter(
        "The type of the value being destructured does not match the expected array type.",
        "Ensure the value is of an array type compatible with the destructure");
}

spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError::SppVariableArrayDestructureArraySizeMismatchError(
    asts::LocalVariableDestructureArrayAst const &var,
    const std::size_t var_size,
    asts::ExpressionAst const &val,
    const std::size_t val_size) {
    AddHeaders(
        24, "SPP Variable Array Destructure Array Size Mismatch Error");
    AddCtxForErr(
        &var,
        "Array destructure variable defined here with size: " + std::to_string(var_size));
    AddErr(
        &val,
        "Value being destructured defined here with size: " + std::to_string(val_size));
    AddFooter(
        "The size of the value being destructured does not match the expected array size.",
        "Ensure the value has the same size as the destructure variable");
}

spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError::SppVariableTupleDestructureTupleTypeMismatchError(
    asts::LocalVariableDestructureTupleAst const &var,
    asts::ExpressionAst const &val,
    asts::TypeAst const &val_type) {
    AddHeaders(
        25, "SPP Variable Tuple Destructure Tuple Type Mismatch Error");
    AddCtxForErr(
        &var,
        "Tuple destructure variable defined here");
    AddErr(
        &val,
        "Value being destructured defined here with type: " + val_type.ToString());
    AddFooter(
        "The type of the value being destructured does not match the expected tuple type.",
        "Ensure the value is of a tuple type compatible with the destructure");
}

spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError::SppVariableTupleDestructureTupleSizeMismatchError(
    asts::LocalVariableDestructureTupleAst const &var,
    const std::size_t var_size,
    asts::ExpressionAst const &val,
    const std::size_t val_size) {
    AddHeaders(
        26, "SPP Variable Tuple Destructure Tuple Size Mismatch Error");
    AddCtxForErr(
        &var,
        "Tuple destructure variable defined here with size: " + std::to_string(var_size));
    AddErr(
        &val,
        "Value being destructured defined here with size: " + std::to_string(val_size));
    AddFooter(
        "The size of the value being destructured does not match the expected tuple size.",
        "Ensure the value has the same size as the destructure variable");
}

spp::analyse::errors::SppVariableObjectDestructureWithBoundMultiSkipError::SppVariableObjectDestructureWithBoundMultiSkipError(
    asts::LocalVariableDestructureObjectAst const &var,
    asts::LocalVariableDestructureSkipMultipleArgumentsAst const &multi_skip) {
    AddHeaders(
        27, "SPP Variable Object Destructure With Bound Multi-Skip Error");
    AddCtxForErr(
        &var,
        "Object destructure variable defined here");
    AddErr(
        &multi_skip,
        "Bound skip multi-argument defined here");
    AddFooter(
        "An object destructure cannot contain a bound skip multi-argument.",
        "Remove the bound skip multi-argument from the destructure");
}

spp::analyse::errors::SppExpressionNotBooleanError::SppExpressionNotBooleanError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    const StrView what) {
    AddHeaders(
        28, "SPP Expression Not Boolean Error");
    AddErr(
        &expr,
        "Expression defined here with type: " + expr_type.ToString());
    AddFooter(
        "This expression must be of boolean type to be used in a " + Str(what) + " context.",
        "Ensure the expression evaluates to a boolean value");
}

spp::analyse::errors::SppExpressionNotGeneratorError::SppExpressionNotGeneratorError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    const StrView what) {
    AddHeaders(
        29, "SPP Expression Not Generator Error");
    AddErr(
        &expr,
        "Expression defined here with type: " + expr_type.ToString());
    AddFooter(
        "This expression must be of generator type to be used in a " + Str(what) + " context.",
        "Ensure the expression evaluates to a generator type");
}

spp::analyse::errors::SppExpressionAmbiguousGeneratorError::SppExpressionAmbiguousGeneratorError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    const StrView what) {
    AddHeaders(
        30, "SPP Expression Ambiguous Generator Error");
    AddErr(
        &expr,
        "Expression defined here with type: " + expr_type.ToString());
    AddFooter(
        "This expression has an ambiguous generator type in a " + Str(what) + " context.",
        "Ensure the expression has a clear and unambiguous generator type");
}

spp::analyse::errors::SppExpressionNotIndexableError::SppExpressionNotIndexableError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    const StrView what) {
    AddHeaders(
        38, "SPP Expression Not Indexable Error");
    AddErr(
        &expr,
        "Expression defined here with type: " + expr_type.ToString());
    AddFooter(
        "This expression must be of an indexable type to be used in a " + Str(what) + " context.",
        "Ensure the expression evaluates to an indexable type (e.g., superimposes IndexRef/IndexMut)");
}

spp::analyse::errors::SppExpressionAmbiguousIndexableError::SppExpressionAmbiguousIndexableError(
    asts::Ast const &expr,
    asts::TypeAst const &expr_type,
    const StrView what) {
    AddHeaders(
        39, "SPP Expression Ambiguous Indexable Error");
    AddErr(
        &expr,
        "Expression defined here with type: " + expr_type.ToString());
    AddFooter(
        "This expression has an ambiguous indexable type in a " + Str(what) + " context.",
        "Ensure the expression has a clear and unambiguous indexable type");
}

spp::analyse::errors::SppLoopTooManyControlFlowStatementsError::SppLoopTooManyControlFlowStatementsError(
    asts::TokenAst const &tok_loop,
    asts::LoopControlFlowStatementAst const &stmt,
    const std::size_t num_controls,
    const std::size_t loop_depth) {
    AddHeaders(
        48, "SPP Loop Too Many Control Flow Statements Error");
    AddCtxForErr(
        &tok_loop,
        "Loop defined here with depth: " + std::to_string(loop_depth));
    AddErr(
        &stmt,
        "Control flow statement defined here (" + std::to_string(num_controls) + " total)");
    AddFooter(
        "This loop contains too many control flow statements (break/continue) for its depth.",
        "Reduce the number of control flow statements or increase the loop depth");
}

spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError::SppObjectInitializerMultipleAutofillArgumentsError(
    asts::ObjectInitializerArgumentAst const &arg1,
    asts::ObjectInitializerArgumentAst const &arg2) {
    AddHeaders(
        49, "SPP Object Initializer Multiple Autofill Arguments Error");
    AddCtxForErr(
        &arg1,
        "First autofill argument defined here");
    AddErr(
        &arg2,
        "Second autofill argument defined here");
    AddFooter(
        "An object initializer cannot contain multiple autofill arguments.",
        "Remove one of the autofill arguments");
}

spp::analyse::errors::SppObjectInitializerInvalidArgumentError::SppObjectInitializerInvalidArgumentError(
    asts::ObjectInitializerArgumentAst const &arg) {
    AddHeaders(
        50, "SPP Object Initializer Invalid Argument Error");
    AddErr(
        &arg,
        "Non-identifier shorthand argument defined here");
    AddFooter(
        "This argument in the object initializer is invalid.",
        "Ensure the shorthand argument is an identifier");
}

spp::analyse::errors::SppObjectInitializerGenericWithArgsError::SppObjectInitializerGenericWithArgsError(
    asts::TypeAst const &type,
    asts::ObjectInitializerArgumentAst const &arg) {
    AddHeaders(
        51, "SPP Object Initializer Generic With Arguments Error");
    AddCtxForErr(
        &type,
        "Generic type initialized here");
    AddErr(
        &arg,
        "Argument provided here");
    AddFooter(
        "A generic type cannot be initialized with arguments.",
        "Remove the arguments from the object initializer");
}

spp::analyse::errors::SppArgumentNameInvalidError::SppArgumentNameInvalidError(
    asts::Ast const &target,
    const StrView target_what,
    asts::Ast const &source,
    const StrView source_what) {
    AddHeaders(
        50, "SPP Argument Name Invalid Error");
    AddCtxForErr(
        &target,
        Str(target_what) + " defined here");
    AddErr(
        &source,
        Str(source_what) + " defined here");
    AddFooter(
        "The name of this argument is invalid in the current context.",
        "Ensure the argument name is correct and valid");
}

spp::analyse::errors::SppArgumentMissingError::SppArgumentMissingError(
    asts::Ast const &target,
    const StrView target_what,
    asts::Ast const &source,
    const StrView source_what) {
    AddHeaders(
        51, "SPP Argument Missing Error");
    AddCtxForErr(
        &target,
        Str(target_what) + " defined here");
    AddErr(
        &source,
        Str(source_what) + " defined here");
    AddFooter(
        "A required argument is missing in the current context.",
        "Provide the missing argument");
}

spp::analyse::errors::SppEarlyReturnRequiresTryTypeError::SppEarlyReturnRequiresTryTypeError(
    asts::ExpressionAst const &expr,
    asts::TypeAst const &type) {
    AddHeaders(
        52, "SPP Early Return Requires Try Type Error");
    AddErr(
        &expr,
        "Expression defined here with type: " + type.ToString());
    AddFooter(
        "This expression is used in an early return context, but its type is not a try type.",
        "Change the expression to have a try type or adjust the context");
}

spp::analyse::errors::SppFunctionCallAbstractFunctionError::SppFunctionCallAbstractFunctionError(
    asts::FunctionPrototypeAst const &proto,
    asts::PostfixExpressionOperatorFunctionCallAst const &call) {
    AddHeaders(
        53, "SPP Function Call Abstract Function Error");
    AddCtxForErr(
        &proto,
        "Abstract function prototype defined here");
    AddErr(
        &call,
        "Function call defined here");
    AddFooter(
        "This function call attempts to call an abstract function, which is not allowed.",
        "Ensure the function being called is not abstract");
}

spp::analyse::errors::SppFunctionCallNotImplFunctionError::SppFunctionCallNotImplFunctionError(
    asts::FunctionPrototypeAst const &proto,
    asts::PostfixExpressionOperatorFunctionCallAst const &call) {
    AddHeaders(
        54, "SPP Function Call Not Implemented Function Error");
    AddCtxForErr(
        &proto,
        "Not implemented function prototype defined here");
    AddErr(
        &call,
        "Function call defined here");
    AddFooter(
        "This function call attempts to call a function that is not implemented.",
        "Ensure the function being called has an implementation");
}

spp::analyse::errors::SppFunctionCallTooManyArgumentsError::SppFunctionCallTooManyArgumentsError(
    asts::FunctionPrototypeAst const &proto,
    asts::PostfixExpressionOperatorFunctionCallAst const &call) {
    AddHeaders(
        55, "SPP Function Call Too Many Arguments Error");
    AddCtxForErr(
        &proto,
        "Function prototype defined here");
    AddErr(
        &call,
        "Function call defined here");
    AddFooter(
        "This function call provides more arguments than the function prototype allows.",
        "Reduce the number of arguments in the function call to match the prototype");
}

spp::analyse::errors::SppFunctionFoldTupleElementTypeMismatchError::SppFunctionFoldTupleElementTypeMismatchError(
    asts::TypeAst const &type,
    asts::ExpressionAst const &expr) {
    AddHeaders(
        56, "SPP Function Fold Tuple Element Type Mismatch Error");
    AddCtxForErr(
        &type,
        "Expected type: " + type.ToString());
    AddErr(
        &expr,
        "Tuple element with mismatched type defined here");
    AddFooter(
        "The type of this tuple element does not match the expected type in the fold operation.",
        "Ensure all tuple elements have matching types");
}

spp::analyse::errors::SppFunctionFoldTupleLengthMismatchError::SppFunctionFoldTupleLengthMismatchError(
    asts::ExpressionAst const &first_tup,
    const std::size_t first_length,
    asts::ExpressionAst const &second_tup,
    const std::size_t second_length) {
    AddHeaders(
        57, "SPP Function Fold Tuple Length Mismatch Error");
    AddCtxForErr(
        &first_tup,
        "First tuple defined here with length: " + std::to_string(first_length));
    AddErr(
        &second_tup,
        "Second tuple defined here with length: " + std::to_string(second_length));
    AddFooter(
        "The lengths of these tuples do not match in the fold operation.",
        "Ensure all tuples have the same length");
}

spp::analyse::errors::SppFunctionCallNoValidSignaturesError::SppFunctionCallNoValidSignaturesError(
    asts::PostfixExpressionOperatorFunctionCallAst const &call,
    const StrView sigs,
    const StrView attempted) {
    AddHeaders(
        58, "SPP Function Call No Valid Signatures Error");
    AddErr(
        &call,
        "Function call defined here");
    AddFooter(
        "No valid signatures match this function call.\n\nAvailable signatures: " + Str(sigs) + "\n\nAttempted: (" + Str(attempted) + ")",
        "Adjust the arguments to match one of the available signatures");
}

spp::analyse::errors::SppFunctionCallOverloadAmbiguousError::SppFunctionCallOverloadAmbiguousError(
    asts::PostfixExpressionOperatorFunctionCallAst const &call,
    const StrView sigs,
    const StrView attempted) {
    AddHeaders(
        59, "SPP Function Call Overload Ambiguous Error");
    AddErr(
        &call,
        "Function call defined here");
    AddFooter(
        "The function call is ambiguous between multiple overloads.\n\nAvailable signatures: " + Str(sigs) + "\n\nAttempted: (" + Str(attempted) + ")",
        "Specify types or adjust arguments to resolve the ambiguity");
}

spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError::SppMemberAccessStaticOperatorExpectedError(
    asts::Ast const &lhs,
    asts::TokenAst const &access) {
    AddHeaders(
        60, "SPP Member Access Static Operator Expected Error");
    AddCtxForErr(
        &lhs,
        "Left-hand side defined here");
    AddErr(
        &access,
        "Member access operator defined here");
    AddFooter(
        "A static operator is expected for this member access.",
        "Use a static member or adjust the access accordingly");
}

spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError::SppMemberAccessRuntimeOperatorExpectedError(
    asts::Ast const &lhs,
    asts::TokenAst const &access) {
    AddHeaders(
        61, "SPP Member Access Runtime Operator Expected Error");
    AddCtxForErr(
        &lhs,
        "Left-hand side defined here");
    AddErr(
        &access,
        "Member access operator defined here");
    AddFooter(
        "A runtime operator is expected for this member access.",
        "Use a runtime member or adjust the access accordingly");
}

spp::analyse::errors::SppGenericTypeInvalidUsageError::SppGenericTypeInvalidUsageError(
    asts::Ast const &gen_name,
    asts::TypeAst const &gen_val,
    const StrView what) {
    AddHeaders(
        62, "SPP Generic Type Invalid Usage Error");
    AddCtxForErr(
        &gen_name,
        "Generic name defined here");
    AddErr(
        &gen_val,
        "Generic value used in " + Str(what) + " context");
    AddFooter(
        "This generic type is used invalidly in the " + Str(what) + " context.",
        "Correct the usage of the generic type");
}

spp::analyse::errors::SppAmbiguousMemberAccessError::SppAmbiguousMemberAccessError(
    asts::Ast const &found_field_1,
    asts::Ast const &found_field_2,
    asts::Ast const &field_access) {
    AddHeaders(
        63, "SPP Ambiguous Member Access Error");
    AddCtxForErr(
        &found_field_1,
        "First matching field defined here");
    AddCtxForErr(
        &found_field_2,
        "Second matching field defined here");
    AddErr(
        &field_access,
        "Ambiguous member access defined here");
    AddFooter(
        "The member access is ambiguous due to multiple matching fields.",
        "Qualify the access to resolve the ambiguity");
}

spp::analyse::errors::SppCoroutineContainsRetExprExpressionError::SppCoroutineContainsRetExprExpressionError(
    asts::TokenAst const &fun_tag,
    asts::TokenAst const &ret_stmt) {
    AddHeaders(
        64, "SPP Function Coroutine Contains RetExpr Expression Error");
    AddCtxForErr(
        &fun_tag,
        "Coroutine defined here");
    AddErr(
        &ret_stmt,
        "Return expression defined here");
    AddFooter(
        "A coroutine cannot contain a return expression.",
        "Use yield expressions instead or change to a subroutine");
}

spp::analyse::errors::SppFunctionSubroutineMissingReturnStatementError::SppFunctionSubroutineMissingReturnStatementError(
    asts::Ast const &final_member,
    asts::TypeAst const &return_type) {
    AddHeaders(
        65, "SPP Function Subroutine Missing Return Statement Error");
    AddErr(
        &final_member,
        "Final member defined here");
    AddFooter(
        "This subroutine is missing a return statement for return type: " + return_type.ToString(),
        "Add a return statement at the end of the subroutine");
}

spp::analyse::errors::SppSuperimpositionCyclicExtensionError::SppSuperimpositionCyclicExtensionError(
    asts::TypeAst const &first_extension,
    asts::TypeAst const &second_extension) {
    AddHeaders(
        66, "SPP Superimposition Cyclic Extension Error");
    AddCtxForErr(
        &first_extension,
        "First extension defined here");
    AddErr(
        &second_extension,
        "Second extension causing cycle defined here");
    AddFooter(
        "This superimposition extension creates a cyclic dependency.",
        "Break the cycle by adjusting the extensions");
}

spp::analyse::errors::SppSuperimpositionDoubleExtensionError::SppSuperimpositionDoubleExtensionError(
    asts::TypeAst const &first_extension,
    asts::TypeAst const &second_extension) {
    AddHeaders(
        66, "SPP Superimposition Double Extension Error");
    AddCtxForErr(
        &first_extension,
        "First extension defined here");
    AddErr(
        &second_extension,
        "Second extension defined here");
    AddFooter(
        "A type cannot superimpose the same type more than once.",
        "Remove one of the extensions / merge the blocks");
}

spp::analyse::errors::SppSuperimpositionSelfExtensionError::SppSuperimpositionSelfExtensionError(
    asts::TypeAst const &first_extension,
    asts::TypeAst const &second_extension) {
    AddHeaders(
        67, "SPP Superimposition Self Extension Error");
    AddCtxForErr(
        &first_extension,
        "Extension defined here");
    AddErr(
        &second_extension,
        "Self-extension defined here");
    AddFooter(
        "A type cannot extend itself in superimposition.",
        "Remove the self-extension");
}

spp::analyse::errors::SppSuperimpositionExtensionMethodInvalidError::SppSuperimpositionExtensionMethodInvalidError(
    asts::IdentifierAst const &new_method,
    asts::TypeAst const &super_class) {
    AddHeaders(
        68, "SPP Superimposition Extension Method Invalid Error");
    AddCtxForErr(
        &super_class,
        "Super class defined here");
    AddErr(
        &new_method,
        "Invalid extension method defined here");
    AddFooter(
        "This method is invalid in the superimposition extension.",
        "Remove or correct the invalid method");
}

spp::analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError::SppSuperimpositionExtensionNonVirtualMethodOverriddenError(
    asts::IdentifierAst const &new_method,
    asts::IdentifierAst const &base_method,
    asts::TypeAst const &super_class) {
    AddHeaders(
        69, "SPP Superimposition Extension Non-Virtual Method Overridden Error");
    AddCtxForErr(
        &base_method,
        "Base non-virtual method defined here");
    AddCtxForErr(
        &super_class,
        "Super class defined here");
    AddErr(
        &new_method,
        "Override of non-virtual method defined here");
    AddFooter(
        "Non-virtual methods cannot be overridden in superimposition extensions.",
        "Make the base method virtual or remove the override");
}

spp::analyse::errors::SppSuperimpositionOptionalGenericParameterError::SppSuperimpositionOptionalGenericParameterError(
    asts::GenericParameterAst const &param) {
    AddHeaders(
        70, "SPP Superimposition Optional Generic Parameter Error");
    AddErr(
        &param,
        "Optional generic parameter defined here");
    AddFooter(
        "Optional generic parameters are not allowed in superimposition.",
        "Make the parameter required or remove it");
}

spp::analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError::SppSuperimpositionUnconstrainedGenericParameterError(
    asts::GenericParameterAst const &param) {
    AddHeaders(
        71, "SPP Superimposition Unconstrained Generic Parameter Error");
    AddErr(
        &param,
        "Unconstrained generic parameter defined here");
    AddFooter(
        "Generic parameters must be constrained in superimposition.",
        "Add constraints to the generic parameter");
}

spp::analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError::SppSuperimpositionExtensionTypeStatementInvalidError(
    asts::TypeStatementAst const &stmt,
    asts::TypeAst const &super_class) {
    AddHeaders(
        72, "SPP Superimposition Extension Type Statement Invalid Error");
    AddCtxForErr(
        &super_class,
        "Super class defined here");
    AddErr(
        &stmt,
        "Invalid type statement defined here");
    AddFooter(
        "This type statement is invalid in the superimposition extension.",
        "Remove or correct the type statement");
}

spp::analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError::SppSuperimpositionExtensionCmpStatementInvalidError(
    asts::CmpStatementAst const &stmt,
    asts::TypeAst const &super_class) {
    AddHeaders(
        73, "SPP Superimposition Extension Cmp Statement Invalid Error");
    AddCtxForErr(
        &super_class,
        "Super class defined here");
    AddErr(
        &stmt,
        "Invalid cmp statement defined here");
    AddFooter(
        "This cmp statement is invalid in the superimposition extension.",
        "Remove or correct the cmp statement");
}

spp::analyse::errors::SppAsyncTargetNotFunctionCallError::SppAsyncTargetNotFunctionCallError(
    asts::TokenAst const &async_op,
    asts::Ast const &rhs) {
    AddHeaders(
        74, "SPP Async Target Not Function Call Error");
    AddCtxForErr(
        &async_op,
        "Async operator defined here");
    AddErr(
        &rhs,
        "Target expression defined here");
    AddFooter(
        "The target of an async operation must be a function call.",
        "Ensure the target is a valid function call");
}

spp::analyse::errors::SppDereferenceInvalidExpressionNonBorrowedTypeError::SppDereferenceInvalidExpressionNonBorrowedTypeError(
    asts::TokenAst const &tok_deref,
    asts::ExpressionAst const &expr,
    asts::TypeAst const &type) {
    AddHeaders(
        75, "SPP Dereference Invalid Expression Non-Borrowed Type Error");
    AddCtxForErr(
        &tok_deref,
        "Dereference operator defined here");
    AddErr(
        &expr,
        "Expression with non-borrowed type: " + type.ToString() + " defined here");
    AddFooter(
        "Cannot dereference an expression of a non-borrowed type.",
        "Ensure the expression has a borrowable type (e.g., a reference)");
}

spp::analyse::errors::SppInvalidExpressionNonCopyableTypeError::SppInvalidExpressionNonCopyableTypeError(
    asts::ExpressionAst const &expr,
    asts::TypeAst const &type) {
    AddHeaders(
        76, "SPP Dereference Invalid Expression Non-Copyable Type Error");
    AddErr(
        &expr,
        "Expression with non-copyable type: " + type.ToString() + " defined here");
    AddFooter(
        "Cannot dereference an expression of a non-copyable type.",
        "Ensure the expression has a copyable type");
}

spp::analyse::errors::SppGenericParameterInferredConflictInferredError::SppGenericParameterInferredConflictInferredError(
    asts::Ast const &param,
    asts::Ast const &first_infer,
    asts::Ast const &second_infer) {
    AddHeaders(
        77, "SPP Generic Parameter Inferred Conflict Inferred Error");
    AddCtxForErr(
        &param,
        "Generic parameter defined here");
    AddCtxForErr(
        &first_infer,
        "First inference defined here: " + first_infer.ToString());
    AddErr(
        &second_infer,
        "Conflicting inference defined here: " + second_infer.ToString());
    AddFooter(
        "There is a conflict between inferred types for this generic parameter.",
        "Resolve the inference conflict");
}

spp::analyse::errors::SppGenericParameterNotInferredError::SppGenericParameterNotInferredError(
    asts::Ast const &param,
    asts::Ast const &ctx) {
    AddHeaders(
        79, "SPP Generic Parameter Not Inferred Error");
    AddCtxForErr(
        &ctx,
        "Context defined here");
    AddErr(
        &param,
        "Generic parameter not inferred here");
    AddFooter(
        "The type for this generic parameter could not be inferred.",
        "Provide an explicit type argument");
}

spp::analyse::errors::SppGenericArgumentTooManyError::SppGenericArgumentTooManyError(
    asts::Ast const &param,
    asts::Ast const &owner,
    asts::GenericArgumentAst const &arg) {
    AddHeaders(
        80, "SPP Generic Argument Too Many Error");
    AddCtxForErr(
        &param,
        "Parameter defined here");
    AddCtxForErr(
        &owner,
        "Owner defined here");
    AddErr(
        &arg,
        "Extra generic argument defined here");
    AddFooter(
        "Too many generic arguments provided for this context.",
        "Remove the extra generic argument");
}

spp::analyse::errors::SppMissingMainFunctionError::SppMissingMainFunctionError(
    asts::ModulePrototypeAst const &mod) {
    AddHeaders(
        81, "SPP Missing Main Function Error");
    AddErr(
        &mod,
        "Module defined here");
    AddFooter(
        "The module is missing a 'main' function, which is required as the entry point of the program.",
        "Define a 'main' function in the module");
}

spp::analyse::errors::SppInvalidVoidValueError::SppInvalidVoidValueError(
    asts::ExpressionAst const &expr,
    const StrView what) {
    AddHeaders(
        82, "SPP Invalid Void Value Error");
    AddErr(
        &expr,
        "Expression defined here");
    AddFooter(
        "This expression evaluates to 'Void' and cannot be used in a " + Str(what) + " context.",
        "Ensure the expression has a valid non-void type");
}

spp::analyse::errors::SppBorrowLifetimeIncreaseError::SppBorrowLifetimeIncreaseError(
    asts::Ast const &extension_ast,
    asts::Ast const &lhs_init_definition,
    asts::Ast const &rhs_borrow_definition) {
    AddHeaders(
        83, "SPP Borrow Lifetime Increase Error");
    AddCtxForErr(
        &lhs_init_definition,
        "Left-hand side initialization defined here");
    AddCtxForErr(
        &rhs_borrow_definition,
        "Right-hand side borrow defined here");
    AddErr(
        &extension_ast,
        "Borrow lifetime extension defined here");
    AddFooter(
        "The borrow lifetime of the right-hand side exceeds that of the left-hand side.",
        "Ensure the borrow lifetime does not exceed the initialization lifetime");
}

spp::analyse::errors::SppInvalidComptimeOperationError::SppInvalidComptimeOperationError(
    asts::Ast const &ast) {
    AddHeaders(
        84, "SPP Invalid Comptime Operation Error");
    AddErr(
        &ast,
        "Ast defined here");
    AddFooter(
        "This expression cannot be evaluatable at compile-time for.",
        "Modify the expression to be computable at compile-time");
}

spp::analyse::errors::SppInternalCompilerError::SppInternalCompilerError(
    asts::Ast const &ast,
    const StrView message) {
    AddHeaders(
        99, "SPP Internal Compiler Error");
    AddErr(
        &ast,
        "Ast defined here");
    AddFooter(
        "An internal compiler error has occurred: " + Str(message),
        "Please report this issue to the SPP development team with the relevant code context");
}

spp::analyse::errors::SppGenericConstraintError::SppGenericConstraintError(
    asts::Ast const &constraint,
    asts::Ast const &concrete_type) {
    AddHeaders(
        85, "SPP Generic Constraint Error");
    AddCtxForErr(
        &constraint,
        "Generic constraint required here: " + constraint.ToString());
    AddErr(
        &concrete_type,
        "Concrete type defined here: " + concrete_type.ToString());
    AddFooter(
        "The concrete type does not satisfy the generic constraint.",
        "Ensure the concrete type meets all requirements of the generic constraint");
}

spp::analyse::errors::SppAnnotationTargetNotAnAnnotationError::SppAnnotationTargetNotAnAnnotationError(
    asts::AnnotationAst const &call_site,
    asts::FunctionPrototypeAst const &target_definition) {
    AddHeaders(
        86, "SPP Annotation Target Not An Annotation Error");
    AddCtxForErr(
        &target_definition,
        "Function defined here is missing '!annotation' tag");
    AddErr(
        &call_site,
        "Calling annotation candidate '" + call_site.ToString() + "' here.");
    AddFooter(
        "This annotation cannot be applied because the target is not an annotation.",
        "Ensure the target function is defined with the '!annotation' tag");
}

spp::analyse::errors::SppAnnotationTargetNotACmpFunctionError::SppAnnotationTargetNotACmpFunctionError(
    asts::AnnotationAst const &annotation_marker,
    asts::Ast const &non_function_ast) {
    AddHeaders(
        87, "SPP Annotation Not A Cmp Function Error");
    AddCtxForErr(
        &non_function_ast,
        "Non-cmp-function or non-function defined here");
    AddErr(
        &annotation_marker,
        "Annotation marker '" + annotation_marker.ToString() + "' defined here");
    AddFooter(
        "This annotation cannot be applied because the target is not a cmp function.",
        "Ensure the annotation is applied to a cmp function");
}

spp::analyse::errors::SppCalledAnnotationAppliedToInvalidAstError::SppCalledAnnotationAppliedToInvalidAstError(
    asts::Ast const &invalid_ast,
    asts::Ast const &annotation_call,
    asts::AnnotationAst const &annotation_definition) {
    AddHeaders(
        88, "SPP Called Annotation Applied To Invalid Ast Error");
    AddCtxForErr(
        &annotation_definition,
        "Annotation & possible targets defined here");
    AddErr(
        &annotation_call,
        "Annotation call defined here");
    AddCtxForErr(
        &invalid_ast,
        "Invalid target of annotation defined here");
    AddFooter(
        "This annotation cannot be applied to the target AST node.",
        "Ensure the annotation is applied to a valid target as defined in the annotation's definition");
}

spp::analyse::errors::SppInvalidBinaryFoldExpressionError::SppInvalidBinaryFoldExpressionError(
    asts::Ast const &expr,
    asts::Ast const &tup_type,
    const std::size_t tup_num_elems) {
    AddHeaders(
        89, "SPP Invalid Binary Fold Expression Error");
    AddCtxForErr(
        &expr,
        "Fold expression defined here as type: " + tup_type.ToString());
    AddErr(
        &expr,
        "Fold expression has " + std::to_string(tup_num_elems) + " elements");
    AddFooter(
        "Binary fold expressions must operate on tuples of 2+ elements.",
        "Ensure the fold expression is applied to a tuple with 2 or more elements");
}

spp::analyse::errors::SppAccessViolationError::SppAccessViolationError(
    asts::Ast const &access_site,
    asts::Ast const &symbol_definition,
    const StrView visibility,
    const StrView what) {
    AddHeaders(
        90, "SPP Access Violation Error");
    AddCtxForErr(
        &symbol_definition,
        Str(what) + " defined here with '" + Str(visibility) + "' visibility");
    AddErr(
        &access_site,
        "Illegal access to " + Str(what) + " here");
    AddFooter(
        "The symbol '" + symbol_definition.ToString() + "' has '" + Str(visibility) + "' visibility and cannot be accessed from this context.",
        "Move the access inside the appropriate type/module scope, or increase the symbol's visibility");
}

spp::analyse::errors::SppFunctionOverloadVisibilityMismatchError::SppFunctionOverloadVisibilityMismatchError(
    asts::AnnotationAst const &first_annotation,
    asts::FunctionPrototypeAst const &conflicting_overload) {
    AddHeaders(
        91, "SPP Function Overload Visibility Mismatch Error");
    AddCtxForErr(
        &first_annotation,
        "First visibility annotation defined here");
    AddErr(
        &conflicting_overload,
        "Conflicting overload with different visibility defined here");
    AddFooter(
        "All overloads of a function must have the same visibility annotation.",
        "Ensure every overload of this function uses the same visibility modifier");
}

SPP_MOD_END
