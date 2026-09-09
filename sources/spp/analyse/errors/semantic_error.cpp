module;
#include <spp/macros.hpp>

#define INLINE_INFO(info) \
    (colex::fg_bright_yellow + colex::st_italic) + info + (colex::reset + colex::st_bold + colex::fg_bright_white)

#define INLINE_NOTE(info) \
    (colex::fg_bright_yellow + colex::st_italic) + info + (colex::reset + colex::st_bold + colex::fg_bright_cyan)

#define INLINE_HELP(info) \
    (colex::fg_bright_yellow + colex::st_italic) + info + (colex::reset + colex::st_bold + colex::fg_bright_red)

module spp.analyse.errors.semantic_error;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_destructure_array_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.module_prototype_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_statement_ast;
import colex;

/*
 * Rules for writing error messages.
 *
 * How to write context / error lines:
 * - Names/symbols: declared, defined, introduced.
 * - Syntax constructs: starts here, begins here, here.
 * - References/borrows: used here, referenced here,
 *
 * How to write note lines:
 * - Focus on the rule being violated.
 * - Neutral, not accusatory.
 * - Example: "Cannot mutate immutable variable" is better
 *     than "Attempted to mutate immutable variable".
 *
 * How to write help lines:
 * - Describe exactly what to do by token or keyword.
 */

SPP_MOD_BEGIN
auto spp::analyse::errors::SemanticError::AddHeaders(
  const std::size_t err_code,
  Str &&msg)
  -> void {
  // Add a header to the error.
  ErrorInfo.PushBack({
    static_cast<asts::Ast const*>(nullptr), ErrorInformationKind::HEADER,
    std::move(msg), "E" + std::to_string(err_code)});
}

namespace spp::analyse::errors {
  namespace {
    /**
     * Follow a call expression back to the ast a person actually wrote. A lowered call points at the expression it was
     * generated from, so reporting against the generated node would underline code that appears nowhere in the source.
     * @param ast The ast to unwrap.
     * @return The originally written ast, or @p ast when it is already the written one.
     */
    auto UnwrapFunctionCallAst(
      spp::asts::Ast const *ast)
      -> spp::asts::Ast const* {
      if (const auto fn_call = ast->To<spp::asts::PostfixExpressionOperatorFunctionCallAst>()) {
        if (fn_call->Source.OriginalExpr != fn_call) {
          return UnwrapFunctionCallAst(fn_call->Source.OriginalExpr);
        }
        return fn_call;
      }
      if (const auto pf = ast->To<spp::asts::PostfixExpressionAst>()) {
        return UnwrapFunctionCallAst(pf->Op.get());
      }
      return ast;
    }
  }
}

auto spp::analyse::errors::SemanticError::AddErr(
  asts::Ast const *ast,
  Str &&tag)
  -> void {
  // Add an error information entry for the given AST and tag.
  ErrorInfo.PushBack({
    UnwrapFunctionCallAst(ast), ErrorInformationKind::ERROR,
    std::move(tag), ""_str});
}

auto spp::analyse::errors::SemanticError::AddErrExact(
  asts::Ast const *ast,
  Str &&tag)
  -> void {
  // Add an error information entry for the given AST and tag, without narrowing a call to its argument group.
  ErrorInfo.PushBack({
    ast, ErrorInformationKind::ERROR,
    std::move(tag), ""_str});
}

auto spp::analyse::errors::SemanticError::AddCtxForErr(
  asts::Ast const *ast,
  Str &&tag)
  -> void {
  // Add a context information entry for the given AST and tag.
  ErrorInfo.PushBack({
    UnwrapFunctionCallAst(ast), ErrorInformationKind::CONTEXT,
    std::move(tag), ""_str});
}

auto spp::analyse::errors::SemanticError::AddFooter(
  Str &&note,
  Str &&help)
  -> void {
  // Add a footer to the error with the given note and help message.
  ErrorInfo.PushBack({
    static_cast<asts::Ast const*>(nullptr), ErrorInformationKind::FOOTER,
    std::move(note), std::move(help)});
}

auto spp::analyse::errors::SemanticError::AddWrapped(
  Str &&msg)
  -> void {
  // Add a wrapped error information entry.
  ErrorInfo.PushBack({
    static_cast<asts::Ast const*>(nullptr), ErrorInformationKind::WRAPPED,
    std::move(msg), ""_str});
}

auto spp::analyse::errors::SemanticError::Clone() const
  -> Unique<SemanticError> {
  // Use the copy constructor to clone the error.
  return MakeUnique<SemanticError>(*this);
}

spp::analyse::errors::SppInvalidPrimaryExpressionError::SppInvalidPrimaryExpressionError(
  asts::Ast const &expr) {
  AddHeaders(0, "Invalid Primary Expression Error");
  AddErr(&expr, "Primary expression introduced here");
  AddFooter(
    "The current context requires a primary expression that is not a (non-zero) type or token ast.",
    "Change the expression to a different value expression.");
}

spp::analyse::errors::SppTypeMismatchError::SppTypeMismatchError(
  asts::Ast const &lhs,
  asts::Ast const &lhs_ty,
  asts::Ast const &rhs,
  asts::Ast const &rhs_ty) {
  AddHeaders(1, "Type Mismatch Error");
  AddCtxForErr(&lhs, "Expected type " + INLINE_INFO(lhs_ty.ToString()));
  AddErr(&rhs, "Found type " + INLINE_INFO(rhs_ty.ToString()));
  AddFooter(
    "The two types are not symbolically equal.",
    "Change one of the expression to match the other's type. If using variant types, consider swapping the\n"
    "expressions, so the broader variant is on the left of the comparison.");
}

spp::analyse::errors::SppSecondClassBorrowViolationError::SppSecondClassBorrowViolationError(
  asts::Ast const &expr,
  asts::Ast const &type,
  const StrView ctx) {
  AddHeaders(2, "Second-Class Borrow Violation Error");
  AddCtxForErr(&type, "Second-class borrow type declared here");
  AddErr(&expr, "Expression used here");
  AddFooter(
    "Second-class borrow types cannot be used in the " + INLINE_NOTE(Str(ctx)) + " context.",
    "Use a first-class type ensuring ownership in this context.");
}

spp::analyse::errors::SppCompileTimeConstantError::SppCompileTimeConstantError(
  asts::Ast const &expr) {
  AddHeaders(3, "SPP Compile-Time Constant Error");
  AddErr(&expr, "Non compile-time expression defined here");
  AddFooter(
    "This expression must be a compile-time constant.",
    "Ensure the expression can be evaluated at compile time.");
}

spp::analyse::errors::SppInvalidMutationError::SppInvalidMutationError(
  asts::Ast const &sym,
  asts::Ast const &mutator,
  asts::Ast const &initialization_location,
  const StrView extra) {
  AddHeaders(4, "Invalid Mutation Error");
  AddCtxForErr(&sym, "Symbol immutably defined here");
  AddCtxForErr(&initialization_location, "Initialized here");
  AddErr(&mutator, "Invalid mutation attempted here");
  AddFooter(
    "The symbol " + INLINE_NOTE(sym.ToString()) + " cannot be mutated because it's an " + INLINE_INFO(extra) + ".",
    "Declare the symbol as mutable or remove the mutation.");
}

spp::analyse::errors::SppUninitializedMemoryUseError::SppUninitializedMemoryUseError(
  asts::Ast const &ast,
  asts::Ast const &init_location,
  asts::Ast const &move_location) {
  AddHeaders(5, "Uninitialized Memory Use Error");
  AddCtxForErr(&init_location, "Memory initialized here");
  AddCtxForErr(&move_location, "Memory moved/uninitialized here");
  AddErr(&ast, "Uninitialized memory used here");
  AddFooter(
    "This expression uses memory that is not initialized, or has been moved.",
    "Reinitialize the variable, or use a different variable.");
}

spp::analyse::errors::SppPartiallyInitializedMemoryUseError::SppPartiallyInitializedMemoryUseError(
  asts::Ast const &ast,
  asts::Ast const &,
  asts::Ast const &partial_move_location) {
  AddHeaders(6, "Partially Initialized Memory Use Error");
  AddErr(&ast, "Expression using partially initialized memory here");
  AddFooter(
    "This expression uses memory that is partially initialized.",
    "Fully initialize the symbol before using it.");
  AddCtxForErr(&partial_move_location, "Partial memory move here");
}

spp::analyse::errors::SppMoveFromBorrowedMemoryError::SppMoveFromBorrowedMemoryError(
  asts::Ast const &ast,
  asts::Ast const &,
  asts::Ast const &borrow_location) {
  AddHeaders(7, "Move From Borrowed Memory Error");
  AddErr(&ast, "Expression attempting to move from borrowed memory");
  AddFooter(
    "This expression attempts to move from memory that is currently borrowed.",
    "Ensure the memory is not borrowed when moving from it, or clone it.");
  AddCtxForErr(&borrow_location, "Memory was borrowed here");
}

spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError::SppInconsistentlyInitializedMemoryUseError(
  asts::Ast const &ast,
  asts::Ast const &branch_1,
  asts::Ast const &branch_2,
  const StrView what) {
  AddHeaders(8, "Inconsistently Initialized Memory Use Error");
  AddErr(&ast, "Variable may not be " + INLINE_INFO(what));
  AddFooter(
    "This expression uses memory that is not consistently " + INLINE_NOTE(what) + " across all branches.",
    "Ensure the memory is " + INLINE_HELP(what) + " in all branches before use.");
  AddCtxForErr(&branch_1, "In this branch, the memory is " + INLINE_INFO(what));
  AddCtxForErr(&branch_2, "In this branch, the memory is not " + INLINE_INFO(what));
}

spp::analyse::errors::SppInconsistentlyEscapingBorrows::SppInconsistentlyEscapingBorrows(
  asts::Ast const &ast,
  asts::Ast const &branch_1,
  asts::Ast const &branch_2) {
  AddHeaders(9, "SPP Inconsistently Escaping Borrows");
  AddCtxForErr(&ast, "Expression using inconsistently escape-borrowed memory defined here");
  AddCtxForErr(&branch_1, "In this branch, the memory escapingly borrowed");
  AddErr(&branch_2, "In this branch, the memory is not escapingly borrowed");
  AddFooter(
    "This expression uses memory that is not consistently escapingly borrowed across all branches.",
    "Ensure the memory is consistently escapingly borrowed in all branches before use.");
}

spp::analyse::errors::SppMemberAccessNonIndexableError::SppMemberAccessNonIndexableError(
  asts::Ast const &lhs,
  asts::Ast const &lhs_type,
  asts::Ast const &access_op) {
  AddHeaders(10, "Member Access Non-Indexable Error");
  AddCtxForErr(&lhs, "Type inferred as " + INLINE_INFO(lhs_type.ToString()));
  AddErr(&access_op, "Member access operator introduced here");
  AddFooter(
    "The expression is not indexable, so member access cannot be performed.",
    "Ensure the left-hand side is an indexable type (e.g., array or tuple).");
}

spp::analyse::errors::SppMemberAccessOutOfBoundsError::SppMemberAccessOutOfBoundsError(
  asts::Ast const &lhs,
  asts::Ast const &,
  const std::size_t n,
  asts::Ast const &access_op) {
  AddHeaders(11, "Member Access Out Of Bounds Error");
  AddCtxForErr(&lhs, "Type has " + INLINE_NOTE(std::to_string(n)) + " elements");
  AddErr(&access_op, "Member access operator introduced here");
  AddFooter(
    "The member access is out of bounds for the given type.",
    "Ensure the accessed member exists within the bounds of the type");
}

spp::analyse::errors::SppCaseBranchElseNotLastError::SppCaseBranchElseNotLastError(
  asts::Ast const &non_last_else_branch,
  asts::Ast const &last_branch) {
  AddHeaders(12, "Case Branch Else Not Last Error");
  AddCtxForErr(&non_last_else_branch, "Non-last " + INLINE_INFO("else") + " branch defined here");
  AddErr(&last_branch, "Last branch defined here");
  AddFooter(
    "The " + INLINE_NOTE("else") + " branch must be the last branch in a case expression.",
    "Move the " + INLINE_HELP("else") + " branch to be the last branch");
}

spp::analyse::errors::SppCaseBranchMissingElseError::SppCaseBranchMissingElseError(
  asts::Ast const &case_expr,
  asts::Ast const &last_branch) {
  AddHeaders(13, "Case Branch Missing Else Error");
  AddCtxForErr(&case_expr, "Case expression introduced here");
  AddErr(&last_branch, "Last branch introduced here");
  AddFooter(
    "A case expression must have an " + INLINE_NOTE("else") + " branch to handle all possible cases.",
    "Add an " + INLINE_HELP("else") + " branch to the case expression.");
}

spp::analyse::errors::SppIdentifierDuplicateError::SppIdentifierDuplicateError(
  asts::Ast const &first_identifier,
  asts::Ast const &duplicate_identifier,
  const StrView what) {
  AddHeaders(14, "Identifier Duplicate Error");
  AddCtxForErr(&first_identifier,
               "First " + INLINE_INFO(Str(what)) + " named " + INLINE_INFO(first_identifier.ToString()) +
               " defined here");
  AddErr(&duplicate_identifier,
         "Duplicate " + INLINE_INFO(Str(what)) + " named " + INLINE_INFO(duplicate_identifier.ToString()) +
         " defined here");
  AddFooter(
    "This " + INLINE_NOTE(Str(what)) + " identifier has already been used.",
    "Rename or remove the duplicate identifier");
}

spp::analyse::errors::SppRecursiveTypeError::SppRecursiveTypeError(
  asts::Ast const &type,
  asts::Ast const &recursion) {
  AddHeaders(15, "Recursive Type Error");
  AddCtxForErr(&type, "Type defined here");
  AddErr(&recursion, "Recursive attribute introduced here");
  AddFooter(
    "This type contains an attribute that causes recursion.",
    "Remove the attribute, change the type, or use smart pointers to prevent the type recursion.");
}

spp::analyse::errors::SppFloatOutOfBoundsError::SppFloatOutOfBoundsError(
  asts::Ast const &literal,
  numex::BigDec const &value,
  numex::BigDec const &lower,
  numex::BigDec const &upper,
  const StrView what) {
  AddHeaders(16, "Float Out Of Bounds Error");
  AddErr(&literal, "Float introduced here with value " + INLINE_INFO(value.Decimal()));
  AddFooter(
    "The value of this float is out of bounds for the " + INLINE_NOTE(Str(what)) + " type.",
    "Ensure the value is within the range: " + INLINE_HELP("[") + INLINE_HELP(lower.Decimal()) + INLINE_HELP(", ") +
    INLINE_HELP(upper.Decimal()) + INLINE_HELP("]") + ".");
}

spp::analyse::errors::SppIntegerOutOfBoundsError::SppIntegerOutOfBoundsError(
  asts::Ast const &literal,
  numex::BigInt const &value,
  numex::BigInt const &lower,
  numex::BigInt const &upper,
  const StrView what) {
  AddHeaders(17, "Integer Out Of Bounds Error");
  AddErr(&literal, "Integer introduced here with value " + INLINE_INFO(value.ToString()));
  AddFooter(
    "The value of this integer is out of bounds for the " + INLINE_NOTE(Str(what)) + " type.",
    "Ensure the value is within the range: " + INLINE_HELP("[") + INLINE_HELP(lower.ToString()) + INLINE_HELP(", ") +
    INLINE_HELP(upper.ToString()) + INLINE_HELP("]") + ".");
}

spp::analyse::errors::SppOrderInvalidError::SppOrderInvalidError(
  const StrView first_what,
  asts::Ast const &first,
  const StrView second_what,
  asts::Ast const &second) {
  AddHeaders(18, "Order Invalid Error");
  AddCtxForErr(&first, INLINE_INFO(Str(first_what)) + " defined here");
  AddErr(&second, INLINE_INFO(Str(second_what)) + " defined here");
  AddFooter(
    "The order of these two asts is invalid.",
    "Switch the order of these asts.");
}

spp::analyse::errors::SppExpansionOfNonTupleError::SppExpansionOfNonTupleError(
  asts::Ast const &unpack,
  asts::Ast const &ast,
  asts::Ast const &type) {
  AddHeaders(19, "Expansion Of Non-Tuple Error");
  AddCtxForErr(&ast, "Expression defined here with type " + INLINE_INFO(type.ToString()));
  AddErr(&unpack, "Unpack operator defined here");
  AddFooter(
    "This expression is being expanded, but it is not of tuple type.",
    "Ensure the expression is of tuple type before expanding.");
}

spp::analyse::errors::SppMemoryOverlapUsageError::SppMemoryOverlapUsageError(
  asts::Ast const &ast,
  asts::Ast const &overlap_ast) {
  AddHeaders(20, "Memory Overlap Usage Error");
  AddCtxForErr(&ast, "Memory region used here");
  AddErr(&overlap_ast, "Overlapping memory region used here");
  AddFooter(
    "This expression uses memory that overlaps with another memory region.",
    "Ensure the memory regions do not overlap.");
}

spp::analyse::errors::SppMultipleSelfParametersError::SppMultipleSelfParametersError(
  asts::Ast const &first_self,
  asts::Ast const &second_self) {
  AddHeaders(21, "Multiple Self Parameters Error");
  AddCtxForErr(&first_self, "First " + INLINE_INFO("self") + " parameter defined here");
  AddErr(&second_self, "Second " + INLINE_INFO("self") + " parameter defined here");
  AddFooter(
    "A function cannot have multiple " + INLINE_NOTE("self") + " parameters.",
    "Remove the second " + INLINE_HELP("self") + " parameters.");
}

spp::analyse::errors::SppMultipleVariadicParametersError::SppMultipleVariadicParametersError(
  asts::Ast const &first_variadic,
  asts::Ast const &second_variadic) {
  AddHeaders(22, "Multiple Variadic Parameters Error");
  AddCtxForErr(&first_variadic, "First " + INLINE_INFO("variadic") + " parameter defined here");
  AddErr(&second_variadic, "Second " + INLINE_INFO("variadic") + " parameter defined here");
  AddFooter(
    "A function cannot have multiple " + INLINE_NOTE("variadic") + " parameters.",
    "Remove one of the " + INLINE_HELP("variadic") + " parameters.");
}

spp::analyse::errors::SppFunctionPrototypeConflictError::SppFunctionPrototypeConflictError(
  asts::Ast const &first_proto,
  asts::Ast const &second_proto) {
  AddHeaders(23, "Function Prototype Conflict Error");
  AddCtxForErr(&first_proto, "First function prototype defined here");
  AddErr(&second_proto, "Conflicting function prototype defined here");
  AddFooter(
    "These two function prototypes conflict with each other.",
    "Rename or modify one of the function prototypes' signature.");
}

spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError::SppFunctionSubroutineContainsGenExpressionError(
  asts::Ast const &fun_tag,
  asts::Ast const &gen_expr) {
  AddHeaders(24, "Function Subroutine Contains Generator Expression Error");
  AddCtxForErr(&fun_tag, "Subroutine defined here");
  AddErr(&gen_expr, "Coroutine value generation introduced here");
  AddFooter(
    "A subroutine cannot contain a " + INLINE_NOTE("gen") + " expression.",
    "Remove the " + INLINE_HELP("gen") + " expression or change the subroutine to a coroutine, by replacing\n"
    "" + INLINE_HELP("fun") + " with " + INLINE_HELP("cor"));
}

spp::analyse::errors::SppYieldedTypeMismatchError::SppYieldedTypeMismatchError(
  asts::Ast const &lhs,
  asts::Ast const &lhs_ty,
  asts::Ast const &rhs,
  asts::Ast const &rhs_ty) {
  AddHeaders(25, "Yielded Type Mismatch Error");
  AddCtxForErr(&lhs, "Yielded type inferred as " + INLINE_INFO(lhs_ty.ToString()));
  AddErr(&rhs, "Expected type inferred as " + INLINE_INFO(rhs_ty.ToString()));
  AddFooter(
    "The type of the yielded value does not match the expected type.",
    "Ensure the yielded value matches the expected type");
}

spp::analyse::errors::SppIdentifierUnknownError::SppIdentifierUnknownError(
  asts::Ast const &name,
  const StrView what,
  std::optional<Str> const &closest) {
  AddHeaders(26, "Identifier Unknown Error");
  AddErr(&name, "Unknown " + INLINE_INFO(Str(what)) + " introduced here" + (closest
           ? " (did you mean '" + *closest + "'?)"
           : ""));
  AddFooter(
    "The " + INLINE_NOTE(Str(what)) + " of " + INLINE_NOTE(name.ToString()) + " is not defined in the current scope.",
    "Define the identifier or correct its name.");
}

spp::analyse::errors::SppSelfIdentifierInvalidContextError::SppSelfIdentifierInvalidContextError(
  asts::Ast const &self) {
  AddHeaders(27, "Self Identifier Invalid Context");
  AddErr(&self, "Invalid " + INLINE_INFO("self") + " identifier introduced here");
  AddFooter(
    "The " + INLINE_NOTE("self") + " identifier can only be used in the context of a method.",
    "Ensure the " + INLINE_HELP("self") + " identifier is used in a valid context, or remove it.");
}

spp::analyse::errors::SppUnreachableCodeError::SppUnreachableCodeError(
  asts::Ast const &member,
  asts::Ast const &next_member) {
  AddHeaders(28, "Unreachable Code Error");
  AddCtxForErr(&member, "Terminating statement introduced here");
  AddErr(&next_member, "Unreachable code here");
  AddFooter(
    "This code is unreachable due to preceding control flow statements.",
    "Remove the unreachable code, or the terminating statement.");
}

spp::analyse::errors::SppInvalidLocalVariableTypeAnnotationError::SppInvalidLocalVariableTypeAnnotationError(
  asts::Ast const &type,
  asts::Ast const &var) {
  AddHeaders(29, "Invalid Local Variable Type Annotation Error");
  AddCtxForErr(&var, "Variable introduced here");
  AddErr(&type, "Invalid type annotation introduced here");
  AddFooter(
    "Type annotations can only be placed on single identifier local variables.",
    "Remove this type annotation.");
}

spp::analyse::errors::SppMultipleRestPatternsError::SppMultipleRestPatternsError(
  asts::Ast const &var,
  asts::Ast const &pattern_1,
  asts::Ast const &pattern_2) {
  AddHeaders(30, "Multiple Rest Patterns Error");
  AddCtxForErr(&var, "Variable destructure introduced here");
  AddCtxForErr(&pattern_1, "First rest pattern introduced here");
  AddErr(&pattern_2, "Second rest pattern introduced here");
  AddFooter(
    "A destructure cannot contain multiple rest patterns.",
    "Remove one of the skip multi-arguments.");
}

spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError::SppVariableArrayDestructureArrayTypeMismatchError(
  asts::Ast const &var,
  asts::Ast const &val,
  asts::Ast const &val_type) {
  AddHeaders(31, "Variable Array Destructure Array Type Mismatch Error");
  AddCtxForErr(&var, "Array destructure introduced here");
  AddErr(&val, "Type inferred as " + INLINE_INFO(val_type.ToString()));
  AddFooter(
    "The type of the value being destructured is not an array.",
    "Change the target to an array, or change the destructure.");
}

spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError::SppVariableArrayDestructureArraySizeMismatchError(
  asts::Ast const &var,
  const std::size_t var_size,
  asts::Ast const &val,
  const std::size_t val_size) {
  AddHeaders(32, "Variable Array Destructure Array Size Mismatch Error");
  AddCtxForErr(&var, "Array destructure introduced with " + INLINE_INFO(std::to_string(var_size)) + " elements");
  AddErr(&val, "Array has " + INLINE_INFO(std::to_string(val_size)) + " elements");
  const auto extra = var_size < val_size ? ", or add the " + INLINE_HELP("..") + " rest pattern" : "";
  AddFooter(
    "The size of the array does not equal the size of the destructure pattern.",
    "Change the size of the destructure pattern" + extra + ".");
}

spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError::SppVariableTupleDestructureTupleTypeMismatchError(
  asts::Ast const &var,
  asts::Ast const &val,
  asts::Ast const &val_type) {
  AddHeaders(33, "Variable Tuple Destructure Tuple Type Mismatch Error");
  AddCtxForErr(&var, "Tuple destructure introduced here");
  AddErr(&val, "Type inferred as " + INLINE_INFO(val_type.ToString()));
  AddFooter(
    "The type of the value being destructured is not a tuple.",
    "Change the target to an tuple, or change the destructure.");
}

spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError::SppVariableTupleDestructureTupleSizeMismatchError(
  asts::Ast const &var,
  const std::size_t var_size,
  asts::Ast const &val,
  const std::size_t val_size) {
  AddHeaders(34, "Variable Tuple Destructure Tuple Size Mismatch Error");
  AddCtxForErr(&var, "Tuple destructure introduced with " + INLINE_INFO(std::to_string(var_size)) + " elements");
  AddErr(&val, "Tuple has " + INLINE_INFO(std::to_string(val_size)) + " elements");
  const auto extra = var_size < val_size ? ", or add the " + INLINE_HELP("..") + " rest pattern" : "";
  AddFooter(
    "The size of the tuple does not equal the size of the destructure pattern.",
    "Change the size of the destructure pattern" + extra + ".");
}

spp::analyse::errors::SppVariableObjectDestructureWithBoundRestPatternError::SppVariableObjectDestructureWithBoundRestPatternError(
  asts::Ast const &var,
  asts::Ast const &rest_pattern) {
  AddHeaders(35, "Variable Object Destructure With Bound Rest Pattern Error");
  AddCtxForErr(&var, "Object destructure introduced here");
  AddErr(&rest_pattern, "Bound rest pattern introduced here");
  AddFooter(
    "An object destructure cannot contain a bound rest pattern.",
    "Remove the bound rest pattern from the destructure.");
}

spp::analyse::errors::SppDestructureSkipsOwnedPartError::SppDestructureSkipsOwnedPartError(
  asts::Ast const &destructure,
  asts::Ast const &value,
  const StrView part) {
  AddHeaders(105, "Destructure Skips Owned Part Error");
  AddCtxForErr(&value, "Value taken apart here");
  AddErr(&destructure, "" + INLINE_INFO(part) + " is left with no owner");
  AddFooter(
    "A destructure cannot drop fields in the background.",
    "Bind " + INLINE_HELP(part) + " and use it, or bind it and drop it explicitly.");
}

spp::analyse::errors::SppPartialMoveOfDestructibleValueError::SppPartialMoveOfDestructibleValueError(
  asts::Ast const &move,
  asts::Ast const &destructor,
  const StrView type_name) {
  AddHeaders(106, "Partial Move Of Destructible Value Error");
  AddCtxForErr(&destructor, "" + INLINE_INFO(type_name) + " is destroyed here");
  AddErr(&move, "Part taken out of it here");
  AddFooter(
    "A type with a specified " + INLINE_NOTE("drop") + " method cannot be partially destructured.",
    "Destructure the whole value instead, or take the part by borrow.");
}

spp::analyse::errors::SppExpressionNotBooleanError::SppExpressionNotBooleanError(
  asts::Ast const &expr,
  asts::Ast const &expr_type,
  const StrView what) {
  AddHeaders(36, "Expression Not Boolean Error");
  AddErr(&expr, "Type inferred as " + INLINE_INFO(expr_type.ToString()));
  AddFooter(
    "This expression must be an owned boolean to be used in a " + INLINE_NOTE(Str(what)) + " context.",
    "Change this expression to a boolean type expression, or take one out of a borrow with " + INLINE_HELP("@") + ".");
}

spp::analyse::errors::SppExpressionNotGeneratorError::SppExpressionNotGeneratorError(
  asts::Ast const &expr,
  asts::Ast const &expr_type,
  const StrView what) {
  AddHeaders(37, "Expression Not Generator Error");
  AddErr(&expr, "Expression inferred as " + INLINE_INFO(expr_type.ToString()));
  AddFooter(
    "This expression must be of generator type to be used in a " + INLINE_NOTE(what) + " context.",
    "Change the expression/type to a generator or a type that superimposes it.");
}

spp::analyse::errors::SppExpressionNotTryError::SppExpressionNotTryError(
  asts::Ast const &expr,
  asts::Ast const &type) {
  AddHeaders(48, "Expression Not Try Error");
  AddErr(&expr, "Expression inferred as " + INLINE_INFO(type.ToString()));
  AddFooter(
    "This expression is used in an early return context, but its type is not a try type.",
    "Change the expression to have a try type.");
}

spp::analyse::errors::SppExpressionAmbiguousGeneratorError::SppExpressionAmbiguousGeneratorError(
  asts::Ast const &expr,
  asts::Ast const &expr_type,
  const StrView what) {
  AddHeaders(38, "SPP Expression Ambiguous Generator Error");
  AddErr(&expr, "Expression inferred as " + INLINE_INFO(expr_type.ToString()));
  AddFooter(
    "This expression has an ambiguous generator type in a " + INLINE_NOTE(what) + " context.",
    "Ensure the expression has a clear and unambiguous generator type (only superimpose one).");
}

spp::analyse::errors::SppExpressionAmbiguousTryError::SppExpressionAmbiguousTryError(
  asts::Ast const &expr,
  asts::Ast const &expr_type,
  const StrView what) {
  AddHeaders(38, "SPP Expression Ambiguous Generator Error");
  AddErr(&expr, "Expression inferred as " + INLINE_INFO(expr_type.ToString()));
  AddFooter(
    "This expression has an ambiguous generator type in a " + INLINE_NOTE(what) + " context.",
    "Ensure the expression has a clear and unambiguous generator type (only superimpose one).");
}

spp::analyse::errors::SppLoopTooManyControlFlowStatementsError::SppLoopTooManyControlFlowStatementsError(
  asts::Ast const &tok_loop,
  asts::Ast const &stmt,
  const std::size_t num_controls,
  const std::size_t loop_depth) {
  AddHeaders(40, "Loop Too Many Control Flow Statements Error");
  AddCtxForErr(
    &tok_loop, "Loop introduced here with at a depth of " + INLINE_INFO(std::to_string(loop_depth)) + " loops");
  AddErr(&stmt,
         "Control flow statement defined here with " + INLINE_INFO(std::to_string(num_controls)) +
         " control flow statements");
  AddFooter(
    "This loop contains too many control flow statements (exit/skip) for its depth.",
    "Reduce the number of control flow statements or increase the loop depth");
}

spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError::SppObjectInitializerMultipleAutofillArgumentsError(
  asts::Ast const &arg1,
  asts::Ast const &arg2) {
  AddHeaders(41, "Object Initializer Multiple Autofill Arguments Error");
  AddCtxForErr(&arg1, "First autofill argument introduced here");
  AddErr(&arg2, "Second autofill argument introduced here");
  AddFooter(
    "An object initializer cannot contain multiple autofill arguments.",
    "Remove one of the autofill arguments.");
}

spp::analyse::errors::SppObjectInitializerInvalidArgumentError::SppObjectInitializerInvalidArgumentError(
  asts::Ast const &arg) {
  AddHeaders(42, "Object Initializer Invalid Argument Error");
  AddErr(&arg, "Non-identifier shorthand argument defined here");
  AddFooter(
    "This argument in the object initializer is invalid.",
    "Shorthand arguments to object initializers must be variables whose identifier name matches a target\n"
    "attribute. Otherwise, use the keyword format " + INLINE_HELP("attr=value") + ".");
}

spp::analyse::errors::SppObjectInitializerVariantError::SppObjectInitializerVariantError(
  asts::Ast const &type) {
  AddHeaders(43, "Object Initializer Variant Error");
  AddCtxForErr(&type, "Variant initialized here");
  AddFooter(
    "A variant type cannot be initialized.",
    "Use the layout: " + INLINE_HELP("let x: VariantType = InnerType()") + ".");
}

spp::analyse::errors::SppObjectInitializerGeneratorError::SppObjectInitializerGeneratorError(
  asts::Ast const &type,
  asts::Ast const &generator_type) {
  AddHeaders(44, "Object Initializer Generator Error");
  AddCtxForErr(&type, "Generator initialized here");
  AddFooter(
    "A generator cannot be initialized, because it superimposes " + INLINE_NOTE(generator_type.ToString()) + ". The "
    "value a generator names is the suspended state of a coroutine, and an object initializer has no such state to "
    "give, so the result could never be resumed.",
    "Produce this type from a " + INLINE_HELP("cor") + " that yields the values, rather than constructing it.");
}

spp::analyse::errors::SppAbstractTypeUseError::SppAbstractTypeUseError(
  asts::Ast const &type,
  asts::Ast const &unimplemented) {
  AddHeaders(45, "Abstract Type Use Error");
  AddCtxForErr(&unimplemented, "Abstract method defined here");
  AddErr(&type, "Abstract type used here");
  AddFooter(
    "A type is abstract if it contains 1 or more non-overridden abstract methods. Abstract\n"
    "types have very restricted usages",
    "Implement the absent abstract method.");
}

spp::analyse::errors::SppArgumentNameInvalidError::SppArgumentNameInvalidError(
  asts::Ast const &target,
  const StrView target_what,
  asts::Ast const &source,
  const StrView source_what) {
  AddHeaders(46, "Argument Name Invalid Error");
  AddCtxForErr(&target, INLINE_INFO(target_what) + " introduced here");
  AddErr(&source, INLINE_INFO(source_what) + " introduced here");
  AddFooter(
    "The name of the " + INLINE_NOTE(source_what) + " is invalid in the current context.",
    "Change the " + INLINE_HELP(source_what) + " to a name that is a " + INLINE_HELP(target_what) + ".");
}

spp::analyse::errors::SppArgumentMissingError::SppArgumentMissingError(
  asts::Ast const &target,
  const StrView target_what,
  asts::Ast const &source,
  const StrView source_what) {
  AddHeaders(47, "Argument Missing Error");
  AddCtxForErr(&target, "Missing " + INLINE_INFO(Str(target_what)) + " defined here");
  AddErr(&source, "Existing " + INLINE_INFO(Str(source_what)) + " defined here");
  AddFooter(
    "A required argument is missing in the current context.",
    "Provide the missing argument.");
}

spp::analyse::errors::SppFunctionCallAbstractFunctionError::SppFunctionCallAbstractFunctionError(
  asts::Ast const &proto,
  asts::Ast const &call) {
  // TODO: This will be changing with the abstract types ticket.
  AddHeaders(49, "SPP Function Call Abstract Function Error");
  AddCtxForErr(&proto, "Abstract function prototype defined here");
  AddErr(&call, "Function call defined here");
  AddFooter(
    "This function call attempts to call an abstract function, which is not allowed.",
    "Ensure the function being called is not abstract");
}

spp::analyse::errors::SppFunctionCallTooManyArgumentsError::SppFunctionCallTooManyArgumentsError(
  asts::Ast const &proto,
  const std::size_t proto_proto_count,
  asts::Ast const &call,
  const std::size_t call_arg_count) {
  AddHeaders(50, "SPP Function Call Too Many Arguments Error");
  AddCtxForErr(
    &proto, "Function prototype defined here with " + INLINE_INFO(std::to_string(proto_proto_count)) + " parameter(s)");
  AddErr(
    &call, "Function call introduced here with " + INLINE_INFO(std::to_string(call_arg_count)) + " argument(s)");
  AddFooter(
    "This function call provides more arguments than the function prototype allows.",
    "Reduce the number of arguments in the function call to match the prototype.");
}

spp::analyse::errors::SppFunctionCallNoValidSignaturesError::SppFunctionCallNoValidSignaturesError(
  asts::Ast const &call,
  const StrView sigs,
  const StrView attempted) {
  AddHeaders(51, "Function Call No Valid Signatures Error");
  AddErr(&call, "Function call defined here");
  AddFooter(
    "No valid signatures match this function call.\n\nAvailable signatures: " + INLINE_NOTE(sigs) + "\n\nAttempted " +
    INLINE_NOTE(attempted),
    "Adjust the arguments to match one of the available signatures. See candidates in more detail below.");
}

spp::analyse::errors::SppFunctionCallOverloadAmbiguousError::SppFunctionCallOverloadAmbiguousError(
  asts::Ast const &call,
  const StrView sigs,
  const StrView attempted) {
  AddHeaders(52, "Function Call Overload Ambiguous Error");
  AddErr(&call, "Function call introduced here");
  AddFooter(
    "The function call is ambiguous between multiple overloads.\n\nAvailable signatures: " + INLINE_NOTE(sigs) +
    "\n\nAttempted: " + INLINE_NOTE(attempted),
    "Specify types or adjust arguments to resolve the ambiguity.");
}

spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError::SppMemberAccessStaticOperatorExpectedError(
  asts::Ast const &lhs,
  asts::Ast const &access,
  const StrView what) {
  AddHeaders(53, "Member Access Static Operator Expected Error");
  AddCtxForErr(&lhs, "" + INLINE_INFO(what) + " identifier introduced here");
  AddErr(&access, "Runtime member access operator " + INLINE_INFO(".") + " introduced here");
  AddFooter(
    "A static operator is required for " + INLINE_NOTE(what) + " member access.",
    "Use the " + INLINE_HELP("::") + " operator, or change the type to a value.");
}

spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError::SppMemberAccessRuntimeOperatorExpectedError(
  asts::Ast const &lhs,
  asts::Ast const &access,
  const StrView what) {
  AddHeaders(54, "Member Access Runtime Operator Expected Error");
  AddCtxForErr(&lhs, "" + INLINE_INFO(what) + " identifier introduced here");
  AddErr(&access, "Static member access operator " + INLINE_INFO("::") + " introduced here");
  AddFooter(
    "A runtime operator is required for " + INLINE_NOTE(what) + " member access.",
    "Use the " + INLINE_HELP(".") + " operator.");
}

spp::analyse::errors::SppGenericTypeInvalidUsageError::SppGenericTypeInvalidUsageError(
  asts::Ast const &gen_name,
  asts::Ast const &gen_val,
  const StrView what) {
  AddHeaders(55, "Generic Type Invalid Usage Error");
  AddCtxForErr(&gen_name, "Generic type defined here");
  AddErr(&gen_val, "Generic value used in " + INLINE_INFO(what) + " context");
  AddFooter(
    "This generic type is used invalidly in the " + INLINE_NOTE(what) + " context.",
    "Correct the usage of the generic type.");
}

spp::analyse::errors::SppAmbiguousMemberAccessError::SppAmbiguousMemberAccessError(
  asts::Ast const &found_field_1,
  asts::Ast const &found_field_2,
  asts::Ast const &field_access) {
  AddHeaders(56, "Ambiguous Member Access Error");
  AddCtxForErr(&found_field_1, "First matching field defined here");
  AddCtxForErr(&found_field_2, "Second matching field defined here");
  AddErr(&field_access, "Ambiguous member access defined here");
  AddFooter(
    "The member access is ambiguous due to multiple matching fields of equal subclass depth.",
    "Not supported at the moment.");
}

spp::analyse::errors::SppCoroutineContainsReturnStatementError::SppCoroutineContainsReturnStatementError(
  asts::Ast const &fun_tag,
  asts::Ast const &ret_stmt) {
  AddHeaders(57, "Function Coroutine Contains Return Statement Error");
  AddCtxForErr(&fun_tag, "Coroutine introduced here");
  AddErr(&ret_stmt, "Return expression introduced here");
  AddFooter(
    "A coroutine cannot contain a return statement.",
    "Use " + INLINE_HELP("gen") + " expressions instead or change the function to a subroutine.");
}

spp::analyse::errors::SppFunctionSubroutineMissingReturnStatementError::SppFunctionSubroutineMissingReturnStatementError(
  asts::Ast const &final_member,
  asts::Ast const &return_type_definition,
  asts::Ast const &return_type) {
  AddHeaders(58, "Function Subroutine Missing Return Statement Error");
  AddCtxForErr(&return_type_definition, "Return type introduced as " + INLINE_INFO(return_type.ToString()));
  AddErr(&final_member, "Final member here");
  AddFooter(
    "This subroutine is missing a return statement for " + INLINE_NOTE(return_type.ToString()) + ".",
    "Add a return statement with an expression at the end of the subroutine.");
}

spp::analyse::errors::SppSuperimpositionCyclicExtensionError::SppSuperimpositionCyclicExtensionError(
  asts::Ast const &first_extension,
  asts::Ast const &second_extension) {
  AddHeaders(59, "Superimposition Cyclic Extension Error");
  AddCtxForErr(&first_extension, "First extension introduced here");
  AddErr(&second_extension, "Second extension causing cycle introduced here");
  AddFooter(
    "This superimposition extension creates a cyclic dependency.",
    "Break the cycle by adjusting the extensions.");
}

spp::analyse::errors::SppShiftAmountOutOfBoundsError::SppShiftAmountOutOfBoundsError(
  asts::Ast const &operation,
  asts::Ast const &amount,
  const StrView type,
  const std::size_t width) {
  AddHeaders(91, "Shift Amount Out Of Bounds Error");
  AddCtxForErr(&amount, "Shift amount introduced here");
  AddErr(&operation, "Shift attempted here");
  const auto width_str = std::to_string(width);
  AddFooter(
    "Shifting by " + width_str + " or more discards every bit of a " + Str(type) + ", so the shift has no meaningful result.",
    "Use a shift amount in the range [0, " + width_str + ").");
}

spp::analyse::errors::SppDivisionByZeroError::SppDivisionByZeroError(
  asts::Ast const &operation,
  asts::Ast const &divisor) {
  AddHeaders(90, "Division By Zero Error");
  AddCtxForErr(&divisor, "Divisor evaluates to zero here");
  AddErr(&operation, "Division attempted here");
  AddFooter(
    "Dividing by zero has no result to compute, so this operation has no value.",
    "Guard the divisor, or use a divisor that cannot be zero.");
}

spp::analyse::errors::SppTypeAliasCyclicError::SppTypeAliasCyclicError(
  asts::Ast const &first_alias,
  asts::Ast const &cyclic_alias) {
  AddHeaders(89, "Type Alias Cyclic Error");
  AddCtxForErr(&first_alias, "Alias chain starts here");
  AddErr(&cyclic_alias, "Alias closing the cycle introduced here");
  AddFooter(
    "This alias resolves back to one it is already defined in terms of, so it names no real type.",
    "Break the cycle by pointing one of the aliases at a class.");
}

spp::analyse::errors::SppSuperimpositionDoubleExtensionError::SppSuperimpositionDoubleExtensionError(
  asts::Ast const &first_extension,
  asts::Ast const &second_extension) {
  AddHeaders(60, "Superimposition Double Extension Error");
  AddCtxForErr(&first_extension, "First extension introduced here");
  AddErr(&second_extension, "Second extension causing duplication introduced here");
  AddFooter(
    "A type cannot superimpose the same type more than once.",
    "Remove one of the extensions or merge the blocks.");
}

spp::analyse::errors::SppSuperimpositionSelfExtensionError::SppSuperimpositionSelfExtensionError(
  asts::Ast const &first_extension,
  asts::Ast const &second_extension) {
  AddHeaders(61, "Superimposition Self Extension Error");
  AddCtxForErr(&first_extension, "Extension introduced here");
  AddErr(&second_extension, "Equal typed super type extended here");
  AddFooter(
    "A type cannot extend itself in superimposition.",
    "Remove the self-extension or use a normal " + INLINE_HELP("sup") + " block.");
}

spp::analyse::errors::SppSuperimpositionExtensionMethodInvalidError::SppSuperimpositionExtensionMethodInvalidError(
  asts::Ast const &new_method,
  asts::Ast const &super_class) {
  AddHeaders(62, "Superimposition Extension Method Invalid Error");
  AddCtxForErr(&super_class, "Super class extended here");
  AddErr(&new_method, "Invalid extension method defined here");
  AddFooter(
    "This method does not exist on the super class.",
    "Remove or correct the invalid method.");
}

spp::analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError::SppSuperimpositionExtensionNonVirtualMethodOverriddenError(
  asts::Ast const &new_method,
  asts::Ast const &base_method,
  asts::Ast const &super_class) {
  AddHeaders(63, "Superimposition Extension Non-Virtual Method Overridden Error");
  AddCtxForErr(&base_method, "Base non-virtual method of " + INLINE_INFO(super_class.ToString()) + " defined here");
  AddCtxForErr(&super_class, "Super class extended here");
  AddErr(&new_method, "Override of non-virtual method defined here");
  AddFooter(
    "Non-virtual methods cannot be overridden in superimposition extensions.",
    "Make the base method virtual or remove the override.");
}

spp::analyse::errors::SppSuperimpositionOptionalGenericParameterError::SppSuperimpositionOptionalGenericParameterError(
  asts::Ast const &param) {
  AddHeaders(64, "Superimposition Optional Generic Parameter Error");
  AddErr(&param, "Optional generic parameter defined here");
  AddFooter(
    "Optional generic parameters are not allowed in superimposition.",
    "Make the parameter required or remove it.");
}

spp::analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError::SppSuperimpositionUnconstrainedGenericParameterError(
  asts::Ast const &param) {
  AddHeaders(65, "Superimposition Unconstrained Generic Parameter Error");
  AddErr(&param, "Unconstrained generic parameter defined here");
  AddFooter(
    "Generic parameters must be constrained in superimposition.",
    "Ensure the generic is being used by the type or supertype, otherwise remove it.");
}

spp::analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError::SppSuperimpositionExtensionTypeStatementInvalidError(
  asts::Ast const &stmt,
  asts::Ast const &super_class) {
  AddHeaders(66, "Superimposition Extension Type Statement Invalid Error");
  AddCtxForErr(&super_class, "Super class defined here");
  AddErr(&stmt, "Invalid type statement defined here");
  AddFooter(
    "This type statement is invalid in the superimposition extension.",
    "Remove or correct the type statement.");
}

spp::analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError::SppSuperimpositionExtensionCmpStatementInvalidError(
  asts::Ast const &stmt,
  asts::Ast const &super_class) {
  AddHeaders(67, "Superimposition Extension Cmp Statement Invalid Error");
  AddCtxForErr(&super_class, "Super class defined here");
  AddErr(&stmt, "Invalid cmp statement defined here");
  AddFooter(
    "This cmp statement is invalid in the superimposition extension.",
    "Remove or correct the cmp statement.");
}

spp::analyse::errors::SppAsyncTargetNotFunctionCallError::SppAsyncTargetNotFunctionCallError(
  asts::Ast const &async_op,
  asts::Ast const &rhs) {
  AddHeaders(68, "Async Target Not Function Call Error");
  AddCtxForErr(&async_op, "Async operator defined here");
  AddErr(&rhs, "Target expression defined here");
  AddFooter(
    "The target of an async operation must be a function call.",
    "Change the target to a function call operation, or remove " + INLINE_HELP("async") + ".");
}

spp::analyse::errors::SppDereferenceNonBorrowedTypeError::SppDereferenceNonBorrowedTypeError(
  asts::Ast const &tok_deref,
  asts::Ast const &expr,
  asts::Ast const &type) {
  AddHeaders(69, "Dereference Non-Borrowed Type Error");
  AddCtxForErr(&tok_deref, "Dereference operator introduced here");
  AddErr(&expr, "Expression inferred as " + INLINE_INFO(type.ToString()) + " defined here");
  AddFooter(
    "Cannot dereference an expression of a non-borrowed type.",
    "Ensure the expression has a borrowable type (e.g., a reference).");
}

spp::analyse::errors::SppNonCopyableTypeError::SppNonCopyableTypeError(
  asts::Ast const &ctx,
  asts::Ast const &expr,
  asts::Ast const &type) {
  AddHeaders(70, "Invalid Expression Non-Copyable Type Error");
  AddCtxForErr(&ctx, "Ast requires a copyable type");
  AddErr(&expr, "Non-copyable underlying type " + INLINE_INFO(type.ToString()));
  AddFooter(
    "Cannot use a non-copyable type here.",
    "Change the expression or superimpose " + INLINE_HELP("Copy") + " over the type.");
}

spp::analyse::errors::SppGenericParameterConflictError::SppGenericParameterConflictError(
  asts::Ast const &param,
  asts::Ast const &first_infer,
  asts::Ast const &second_infer) {
  AddHeaders(71, "Generic Parameter Inferred Conflict Inferred Error");
  AddCtxForErr(&param, "Generic parameter defined here");
  AddCtxForErr(&first_infer, "Generic inferred as " + INLINE_INFO(first_infer.ToString()));
  AddErr(&second_infer, "Generic inferred as " + INLINE_INFO(second_infer.ToString()));
  AddFooter(
    "There is a conflict between inferred types for this generic parameter.",
    "Resolve the inference conflict.");
}

spp::analyse::errors::SppGenericParameterNotInferredError::SppGenericParameterNotInferredError(
  asts::Ast const &param,
  asts::Ast const &ctx) {
  AddHeaders(72, "Generic Parameter Not Inferred Error");
  AddCtxForErr(&ctx, "Context parameter introduced here");
  AddErr(&param, "Generic parameter not inferred here");
  AddFooter(
    "The type for this generic parameter could not be inferred.",
    "Provide an explicit type argument for the generic parameter.");
}

spp::analyse::errors::SppGenericArgumentTooManyError::SppGenericArgumentTooManyError(
  asts::Ast const &param,
  asts::Ast const &owner,
  asts::Ast const &arg) {
  AddHeaders(73, "Generic Argument Too Many Error");
  AddCtxForErr(&param, "Generic parameter defined here");
  AddCtxForErr(&owner, "Owner defined here");
  AddErr(&arg, "Extra generic argument defined here");
  AddFooter(
    "Too many generic arguments provided for this context.",
    "Remove the extra generic argument.");
}

spp::analyse::errors::SppMissingMainFunctionError::SppMissingMainFunctionError() {
  AddHeaders(74, "Missing Main Function Error");
  AddFooter(
    "The module is missing a 'main' function, which is required as the entry point of the program.",
    "Define a 'main' function in the module");
}

spp::analyse::errors::SppInvalidVoidValueError::SppInvalidVoidValueError(
  asts::Ast const &expr,
  const StrView what) {
  AddHeaders(75, "Invalid Void Value Error");
  AddErr(&expr, "Expression inferred as " + INLINE_INFO("Void"));
  AddFooter(
    "Void expressions cannot be used in " + INLINE_NOTE(what) + " contexts.",
    "Ensure the expression has a valid non-void type, or move it out of the " + INLINE_HELP(what) + " context.");
}

spp::analyse::errors::SppBorrowLifetimeIncreaseError::SppBorrowLifetimeIncreaseError(
  asts::Ast const &extension_ast,
  asts::Ast const &lhs_init_definition,
  asts::Ast const &rhs_borrow_definition) {
  AddHeaders(76, "Borrow Lifetime Increase Error");
  AddCtxForErr(&lhs_init_definition, "Left-hand side initialized here");
  AddCtxForErr(&rhs_borrow_definition, "Right-hand side borrow here");
  AddErr(&extension_ast, "Borrow lifetime extension defined here");
  AddFooter(
    "The borrow lifetime of the right-hand side exceeds that of the left-hand side.",
    "Ensure the borrow lifetime does not exceed the initialization lifetime.");
}

spp::analyse::errors::SppInvalidComptimeOperationError::SppInvalidComptimeOperationError(
  asts::Ast const &ast) {
  AddHeaders(77, "Invalid Comptime Operation Error");
  AddErr(&ast, "Expression introduced here");
  AddFooter(
    "This expression cannot be evaluated at compile-time.",
    "Modify the expression/target to be compile-time compatible.");
}

spp::analyse::errors::SppInternalCompilerError::SppInternalCompilerError(
  asts::Ast const &ast,
  const StrView message) {
  AddHeaders(1000, "Internal Compiler Error");
  AddErr(&ast, "Ast defined here");
  AddFooter(
    "An internal compiler error has occurred: " + INLINE_NOTE(message),
    "Please report this issue to the SPP development team with the relevant code context");
}

spp::analyse::errors::SppGenericConstraintError::SppGenericConstraintError(
  asts::Ast const &constraint,
  asts::Ast const &concrete_type) {
  AddHeaders(78, "Generic Constraint Error");
  AddCtxForErr(&constraint, "Generic constraint introduced here as " + INLINE_INFO(constraint.ToString()));
  AddErr(&concrete_type, "Concrete type provided here as " + INLINE_INFO(concrete_type.ToString()));
  AddFooter(
    "The concrete type does not satisfy the generic constraint.",
    "Ensure the concrete type meets all requirements of the generic constraint.");
}

spp::analyse::errors::SppAnnotationTargetNotAnAnnotationError::SppAnnotationTargetNotAnAnnotationError(
  asts::Ast const &call_site,
  asts::Ast const &target_definition) {
  AddHeaders(79, "Annotation Target Not An Annotation Error");
  AddCtxForErr(&target_definition, "Function defined here is missing " + INLINE_INFO("!annotation") + " tag");
  AddErr(&call_site, "Calling annotation candidate " + INLINE_INFO(call_site.ToString()) + " here.");
  AddFooter(
    "This annotation cannot be applied because the target is not an annotation.",
    "Ensure the target function is defined with the '!annotation' tag.");
}

spp::analyse::errors::SppAnnotationTargetNotACmpFunctionError::SppAnnotationTargetNotACmpFunctionError(
  asts::Ast const &annotation_marker,
  asts::Ast const &non_function_ast) {
  AddHeaders(80, "Annotation Not A Cmp Function Error");
  AddCtxForErr(&non_function_ast, "Non-cmp-function defined here");
  AddErr(&annotation_marker, "Annotation marker applied here");
  AddFooter(
    "This annotation cannot be applied because the target is not a cmp function.",
    "Ensure the annotation is applied to a cmp function.");
}

spp::analyse::errors::SppCalledAnnotationAppliedToInvalidAstError::SppCalledAnnotationAppliedToInvalidAstError(
  asts::Ast const &invalid_ast,
  asts::Ast const &annotation_call,
  asts::Ast const &annotation_definition) {
  AddHeaders(81, "Called Annotation Applied To Invalid Ast Error");
  AddCtxForErr(&annotation_definition, "Annotation & possible targets defined here");
  AddCtxForErr(&annotation_call, "Annotation applied here");
  AddErr(&invalid_ast, "Invalid target of annotation used here");
  AddFooter(
    "Annotation targets must be satisfied by the AST they are applied to.",
    "Remove the annotation, or add the AST's type to the annotation's targets.");
}

spp::analyse::errors::SppUnitTestInvalidSignatureError::SppUnitTestInvalidSignatureError(
  asts::Ast const &annotation,
  asts::Ast const &fun_name,
  const StrView requirement) {
  AddHeaders(92, "Unit Test Invalid Signature Error");
  AddCtxForErr(&annotation, "Marked as a unit test here");
  AddErr(&fun_name, "Unit test declared here");
  AddFooter(
    "A unit test is run by the test harness, which has nothing to pass it and nowhere to put a result, so it must be "
    + INLINE_NOTE("a plain 'fun' taking no parameters and returning 'Void'") + ". This one " + INLINE_NOTE(requirement)
    + ".",
    "Remove the " + INLINE_HELP("unit_test") + " annotation, or change the signature to " + INLINE_HELP(
      "fun name() -> Void") + ".");
}

spp::analyse::errors::SppFfiGenericParameterError::SppFfiGenericParameterError(
  asts::Ast const &annotation,
  asts::Ast const &generic_parameter,
  const StrView symbol) {
  AddHeaders(101, "Ffi Generic Parameter Error");
  AddCtxForErr(&annotation, "Bound to " + Str(symbol) + " here");
  AddErr(&generic_parameter, "Generic parameter on an ffi function");
  AddFooter(
    "A c function has one prototype, so a binding for one has to have exactly one signature too. A generic parameter "
    "gives it more than one - " + INLINE_NOTE("f[T](x: T)") + " lowers differently for every " +
    INLINE_NOTE("T") + " it is called at - and only the first could be emitted under the symbol the linker resolves.",
    "Name the c type the binding actually takes, or declare one binding per c prototype, the way " +
    INLINE_NOTE("fcntl") + " is split into " + INLINE_NOTE("fcntl_get") + ", " + INLINE_NOTE("fcntl_set") + " and " +
    INLINE_NOTE("fcntl_lock") + ". A callable is taken as " + INLINE_NOTE("CClosure[Ts, R]") + ", the concrete pair c "
    "receives, which " + INLINE_NOTE("CClosure::from") + " makes from a closure.");
}

spp::analyse::errors::SppEmptyBodyRequiredError::SppEmptyBodyRequiredError(
  asts::Ast const &annotation,
  asts::Ast const &member,
  const StrView what,
  const StrView reason) {
  AddHeaders(102, "Empty Body Required Error");
  AddCtxForErr(&annotation, "Marked as " + Str(what) + " here");
  AddErr(&member, "Written inside the body");
  // The annotation's own "ToString" renders its argument group too - "!zero_type()" for one that takes none - so the
  // help names it the way it is written instead.
  const auto *as_annotation = dynamic_cast<asts::AnnotationAst const*>(&annotation);
  auto marker = as_annotation != nullptr ? "!" + as_annotation->Name->ToString() : annotation.ToString();
  AddFooter(
    "The body of " + INLINE_NOTE(Str(what)) + " must be empty: " + Str(reason) + ".",
    "Empty the body, or remove the " + INLINE_HELP(std::move(marker)) + " annotation.");
}

spp::analyse::errors::SppUnitTestNotCallableError::SppUnitTestNotCallableError(
  asts::Ast const &call_site,
  asts::Ast const &annotation) {
  AddHeaders(93, "Unit Test Not Callable Error");
  AddCtxForErr(&annotation, "Marked as a unit test here");
  AddErr(&call_site, "Called here");
  AddFooter(
    "A unit test is only ever entered by the test harness. Calling one from s++ code would run it as part of the "
    "program rather than as part of the suite.",
    "Move the shared code into an ordinary function and call that from both.");
}

spp::analyse::errors::SppInvalidBinaryFoldExpressionError::SppInvalidBinaryFoldExpressionError(
  asts::Ast const &expr,
  asts::Ast const &tup_type,
  const std::size_t tup_num_elems) {
  AddHeaders(82, "Invalid Binary Fold Expression Error");
  AddCtxForErr(&expr, "Fold operand expression inferred as: " + INLINE_INFO(tup_type.ToString()));
  AddErr(&expr, "Fold expression has " + INLINE_INFO(std::to_string(tup_num_elems)) + " element(s)");
  AddFooter(
    "Binary fold expressions must operate on tuples of 2+ elements.",
    "Ensure the fold expression is applied to a tuple with 2 or more elements.");
}

spp::analyse::errors::SppAccessViolationError::SppAccessViolationError(
  asts::Ast const &access_site,
  asts::Ast const &symbol_definition,
  const StrView visibility,
  const StrView what) {
  AddHeaders(83, "Access Violation Error");
  AddCtxForErr(&symbol_definition,
               INLINE_INFO(what) + " defined here with '" + INLINE_INFO(visibility) + "' visibility");
  AddErr(&access_site, "Illegal access to " + INLINE_INFO(what) + " here");
  AddFooter(
    "The symbol '" + INLINE_NOTE(symbol_definition.ToString()) + "' has '" + INLINE_NOTE(visibility) +
    "' visibility and cannot be accessed from this context.",
    "Move the access inside the appropriate type/module scope, or increase the symbol's visibility.");
}

spp::analyse::errors::SppFunctionOverloadVisibilityMismatchError::SppFunctionOverloadVisibilityMismatchError(
  asts::Ast const &first_annotation,
  asts::Ast const &conflicting_overload,
  asts::Ast const &conflicting_annotation) {
  AddHeaders(84, "Function Overload Visibility Mismatch Error");
  AddCtxForErr(
    &first_annotation,
    "First visibility annotation defined here" + INLINE_INFO(first_annotation.ToString()));
  AddCtxForErr(
    &conflicting_annotation,
    "Second visibility annotation defined here as " + INLINE_INFO(conflicting_annotation.ToString()));
  AddErr(&conflicting_overload, "Conflicting overload with second visibility defined here");
  AddFooter(
    "All overloads of a function must have the same visibility annotation.",
    "Ensure every overload of this function uses the same visibility modifier.");
}

spp::analyse::errors::SppMovingEscapingBorrowedMemoryError::SppMovingEscapingBorrowedMemoryError(
  asts::Ast const &container,
  asts::Ast const &where_moved) {
  AddHeaders(85, "Moving Escaping Borrow Memory Error");
  AddCtxForErr(&container, "Escaping borrow contained by this symbol");
  AddErr(&where_moved, "Attempted to move underlying value here");
  AddFooter(
    "Non-copyable values that are borrowed with an escaping context cannot be moved from.",
    "Remove the move operation, make the type copyable, or restructure your borrows");
}

spp::analyse::errors::SppMovingComptimeConstantMemoryError::SppMovingComptimeConstantMemoryError(
  asts::Ast const &ast,
  asts::Ast const &move_location) {
  AddHeaders(86, "Moving Compile-Time Constant Memory Error");
  AddCtxForErr(&ast, "Compile-time constant defined here");
  AddErr(&move_location, "Attempted to move compile-time constant here");
  AddFooter(
    "Compile-time constants cannot be moved from.",
    "Remove the move operation or ensure the value is not a compile-time constant.");
}

spp::analyse::errors::SppHigherOrderGenericsNotSupportedError::SppHigherOrderGenericsNotSupportedError(
  asts::Ast const &ast,
  asts::Ast const &generic_arg_group) {
  AddHeaders(87, "Higher-Order Generics Not Supported Error");
  AddCtxForErr(&ast, "Generic type used here");
  AddErr(&generic_arg_group, "Generic argument group defined here");
  AddFooter(
    "Higher-order generics are not yet supported.",
    "Remove the generic argument group from the generic type.");
}

spp::analyse::errors::SppGeneratedCodeError::SppGeneratedCodeError(
  asts::Ast const &ast,
  Str &&wrapped_error) {
  AddHeaders(1001, "Generated Code Error");
  AddWrapped(std::move(wrapped_error));
  AddErr(&ast, "Generated code expanded from here");
  AddFooter(
    "An error occurred in generated code.",
    "Refer to the wrapped error.");
}

spp::analyse::errors::SppCharLiteralOutOfBoundsError::SppCharLiteralOutOfBoundsError(
  asts::Ast const &literal,
  const std::uint32_t code_point) {
  AddHeaders(88, "Char Literal Out Of Bounds Error");
  AddErr(&literal,
         "Byte-prefixed char literal introduced here with code point " + INLINE_INFO(std::to_string(code_point)));
  AddFooter(
    "A byte-prefixed char literal (" + INLINE_NOTE("b'...'") + ") must decode to a single byte, but this one decodes "
    "to a Unicode code point outside " + INLINE_NOTE("0..255") + ".",
    "Remove the " + INLINE_HELP("b") + " byte-prefix, or use a character whose code point fits in a single byte.");
}

spp::analyse::errors::SppLinearValueNotConsumedError::SppLinearValueNotConsumedError(
  asts::Ast const &symbol_definition,
  asts::Ast const &exit_point,
  const StrView symbol_name,
  const StrView type_name,
  const StrView exit_what) {
  AddHeaders(94, "Linear Value Not Consumed Error");
  AddCtxForErr(&symbol_definition, "Value of type " + INLINE_INFO(Str(type_name)) + " introduced here");
  AddErr(&exit_point, Str(exit_what) + " reached with " + INLINE_INFO(Str(symbol_name)) + " still holding it");
  AddFooter(
    "A value of a non-" + INLINE_NOTE("Copy") + " type must be used exactly once, so no symbol can still own one "
    "when its scope ends.",
    "Move " + INLINE_HELP(Str(symbol_name)) + " into a consuming function, return it, or take it apart with "
    + INLINE_HELP("let " + Str(type_name) + "(..) = " + Str(symbol_name)) + ".");
}

spp::analyse::errors::SppDiscardedValueError::SppDiscardedValueError(
  asts::Ast const &expr,
  const StrView type_name) {
  AddHeaders(95, "Discarded Value Error");
  AddErrExact(&expr, "Expression of type " + INLINE_INFO(Str(type_name)) + " produces a value nothing takes");
  AddFooter(
    "An expression in statement position produces a value that nothing consumes; only " + INLINE_NOTE("Void")
    + " and " + INLINE_NOTE("Never") + " may be discarded.",
    "Bind the value with " + INLINE_HELP("let") + ", return it with " + INLINE_HELP("ret") + ", or remove the "
    "expression.");
}

spp::analyse::errors::SppLinearValueSkippedInDestructureError::SppLinearValueSkippedInDestructureError(
  asts::Ast const &skip,
  asts::Ast const &destructure,
  const StrView attr_name,
  const StrView type_name) {
  AddHeaders(96, "Linear Value Skipped In Destructure Error");
  AddCtxForErr(&destructure, "Destructure of " + INLINE_INFO(destructure.ToString()) + " here");
  AddErr(&skip, "Skip covers " + INLINE_INFO(Str(attr_name)) + " of non-Copy type " + INLINE_INFO(Str(type_name)));
  AddFooter(
    "A destructure consumes the whole value, so an attribute a skip covers is discarded rather than used.",
    "Bind " + INLINE_HELP(Str(attr_name)) + " explicitly in the destructure instead of skipping it.");
}

spp::analyse::errors::SppDeferTerminatesError::SppDeferTerminatesError(
  asts::Ast const &tok_defer,
  asts::Ast const &expr) {
  AddHeaders(97, "Defer Terminates Error");
  AddCtxForErr(&tok_defer, "Deferred here");
  AddErrExact(&expr, "Expression leaves the scope rather than running in it");
  AddFooter(
    "A deferred expression runs because its scope is being left, so it cannot leave that scope itself.",
    "Remove the " + INLINE_HELP("ret") + ", " + INLINE_HELP("exit") + ", " + INLINE_HELP("skip") + " or "
    + INLINE_HELP("?") + " from the deferred expression; handle the failure where the value is still in hand.");
}

spp::analyse::errors::SppFeatureNotYetSupportedError::SppFeatureNotYetSupportedError(
  const NotYetSupportedFeature feature,
  asts::Ast const &context,
  asts::Ast const &site) {
  // One entry per feature: what to underline, and what to say. Adding a feature is an enumerator and a case here.
  struct Text { Str Ctx, Site, Note, Help; };
  const auto text = [&]() -> Text {
    switch (feature) {
      case NotYetSupportedFeature::NestedTypeBeforeSupScopes:
        return {
          "Type named here",
          "Nested type not available this early",
          "A nested type is declared inside a " + INLINE_NOTE("sup") + " block, which only becomes part of its owner "
          "once superimposition scopes are attached - and that happens in the same pass that resolves the types "
          "written in a signature. Naming one here would need that pass split in two, which is not done yet.",
          "Name the type the alias resolves to, or move the use into a function body, where it does work."};

      default:
        std::unreachable();
    }
  }();

  AddHeaders(100, "Feature Not Yet Supported Error");
  AddCtxForErr(&context, Str(text.Ctx));
  AddErrExact(&site, Str(text.Site));
  AddFooter(Str(text.Note), Str(text.Help));
}

spp::analyse::errors::SppDeferConsumesMovedValueError::SppDeferConsumesMovedValueError(
  asts::Ast const &deferred,
  asts::Ast const &consumed_at,
  const StrView symbol_name,
  const StrView exit_what) {
  AddHeaders(99, "Defer Consumes Moved Value Error");
  AddCtxForErr(&deferred, "Deferred here, so it runs at every exit of this scope");
  AddErrExact(
    &consumed_at, Str(exit_what) + " reached with " + INLINE_INFO(Str(symbol_name)) + " already consumed here");
  AddFooter(
    "A deferred expression is not conditional - it is emitted at every exit,\n\t"
    "with nothing at runtime to record that one path already consumed the\n\t"
    "value - so this one would consume it a second time.",
    "Discharge " + INLINE_HELP(Str(symbol_name)) + " in each branch that does\n\t"
    "not already consume it, rather than deferring it for all of them.");
}

spp::analyse::errors::SppDeferInCompileTimeFunctionError::SppDeferInCompileTimeFunctionError(
  asts::Ast const &tok_defer) {
  AddHeaders(98, "Defer In Compile-Time Function Error");
  AddErr(&tok_defer, "Deferred here, inside a function evaluated at compile time");
  AddFooter(
    "Compile-time evaluation has no scope exit to run a deferred expression at.",
    "Run the expression where it is needed instead of deferring it, or make the function a runtime one.");
}

spp::analyse::errors::SppGenOnceFinishesWithoutYieldingError::SppGenOnceFinishesWithoutYieldingError(
  asts::Ast const &ret_stmt) {
  AddHeaders(104, "GenOnce Finishes Without Yielding Error");
  AddErr(&ret_stmt, "Finishes the coroutine here, on a path that has not yielded");
  AddFooter(
    "A 'GenOnce' is guaranteed to yield exactly once, which is what lets a caller read it as the value it yields\n\t"
    "rather than as a generator to be tested. A 'ret' reached before any 'gen' breaks that guarantee.",
    "Yield a value on this path before returning, or make the coroutine a 'Gen', which may yield nothing.");
}

SPP_MOD_END
