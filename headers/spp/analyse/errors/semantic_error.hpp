#pragma once

#include <spp/utils/errors.hpp>
#include <spp/asts/_fwd.hpp>

#include <boost/multiprecision/cpp_dec_float.hpp>


namespace spp::analyse::errors {
    struct SppAnnotationInvalidApplicationError;
    struct SppAnnotationConflictError;
    struct SppAnnotationInvalidError;
    struct SppExpressionTypeInvalidError;
    struct SppTypeMismatchError;
    struct SppSecondClassBorrowViolationError;
    struct SppCompileTimeConstantError;
    struct SppInvalidMutationError;
    struct SppUninitializedMemoryUseError;
    struct SppPartiallyInitializedMemoryUseError;
    struct SppMoveFromBorrowedMemoryError;
    struct SppMoveFromPinnedMemoryError;
    struct SppMoveFromPinLinkedMemoryError;
    struct SppInconsistentlyInitializedMemoryUseError;
    struct SppInconsistentlyPinnedMemoryUseError;
    struct SppCompoundAssignmentTargetError;
    struct SppMemberAccessNonIndexableError;
    struct SppCaseBranchMultipleDestructuresError;
    struct SppCaseBranchElseNotLastError;
    struct SppCaseBranchMissingElseError;
    struct SppIdentifierDuplicateError;
    struct SppRecursiveTypeError;
    struct SppCoroutineInvalidReturnTypeError;
    struct SppNumberOutOfBoundsError;
    struct SppOrderInvalidError;
    struct SppExpansionOfNonTupleError;
    struct SppMemoryOverlapUsageError;
    struct SppMultipleSelfParametersError;

    auto add_header(std::size_t err_code, std::string &&msg) -> std::string;
    auto add_error(std::size_t pos, std::string &&tag) -> std::string;
    auto add_context_for_error(std::size_t pos, std::string &&msg) -> std::string;
    auto add_footer(std::string &&note, std::string &&help) -> std::string;
}


struct spp::analyse::errors::SppAnnotationInvalidApplicationError final : spp::utils::errors::SemanticError {
    explicit SppAnnotationInvalidApplicationError(asts::AnnotationAst const &annotation, asts::Ast const &ctx, std::string &&block_list);
};


struct spp::analyse::errors::SppAnnotationConflictError final : spp::utils::errors::SemanticError {
    explicit SppAnnotationConflictError(asts::AnnotationAst const &first_annotation, asts::AnnotationAst const &conflicting_annotation, asts::Ast const &ctx);
};


struct spp::analyse::errors::SppAnnotationInvalidError final : spp::utils::errors::SemanticError {
    explicit SppAnnotationInvalidError(asts::AnnotationAst const &annotation);
};


struct spp::analyse::errors::SppExpressionTypeInvalidError final : spp::utils::errors::SemanticError {
    explicit SppExpressionTypeInvalidError(asts::ExpressionAst const &expr);
};


struct spp::analyse::errors::SppTypeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppTypeMismatchError(asts::Ast const &lhs, asts::TypeAst const &lhs_ty, asts::Ast const &rhs, asts::TypeAst const &rhs_ty);
};


struct spp::analyse::errors::SppSecondClassBorrowViolationError final : spp::utils::errors::SemanticError {
    explicit SppSecondClassBorrowViolationError(asts::Ast const &expr, asts::ConventionAst const &conv, std::string_view ctx);
};


struct spp::analyse::errors::SppCompileTimeConstantError final : spp::utils::errors::SemanticError {
    explicit SppCompileTimeConstantError(asts::ExpressionAst const &expr);
};


struct spp::analyse::errors::SppInvalidMutationError final : spp::utils::errors::SemanticError {
    explicit SppInvalidMutationError(asts::Ast const &sym, asts::Ast const &mutator, asts::Ast const &initialization_location);
};


struct spp::analyse::errors::SppUninitializedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppUninitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location);
};


struct spp::analyse::errors::SppPartiallyInitializedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppPartiallyInitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &partial_move_location);
};


struct spp::analyse::errors::SppMoveFromBorrowedMemoryError final : spp::utils::errors::SemanticError {
    explicit SppMoveFromBorrowedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &move_location, asts::Ast const &borrow_location);
};


struct spp::analyse::errors::SppMoveFromPinnedMemoryError final : spp::utils::errors::SemanticError {
    explicit SppMoveFromPinnedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location, asts::Ast const &pin_location);
};


struct spp::analyse::errors::SppMoveFromPinLinkedMemoryError final : spp::utils::errors::SemanticError {
    explicit SppMoveFromPinLinkedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location, asts::Ast const &pin_location, asts::Ast const &pin_init_location);
};


struct spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppInconsistentlyInitializedMemoryUseError(asts::ExpressionAst const &ast, std::pair<asts::CaseExpressionBranchAst*, bool> branch_1, std::pair<asts::CaseExpressionBranchAst*, bool> branch_2, std::string_view what);
};


struct spp::analyse::errors::SppInconsistentlyPinnedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppInconsistentlyPinnedMemoryUseError(asts::ExpressionAst const &ast, std::pair<asts::CaseExpressionBranchAst*, bool> branch_1, std::pair<asts::CaseExpressionBranchAst*, bool> branch_2);
};


struct spp::analyse::errors::SppCompoundAssignmentTargetError final : spp::utils::errors::SemanticError {
    explicit SppCompoundAssignmentTargetError(asts::ExpressionAst const &lhs);
};


struct spp::analyse::errors::SppMemberAccessNonIndexableError final : spp::utils::errors::SemanticError {
    explicit SppMemberAccessNonIndexableError(asts::ExpressionAst const &lhs, asts::TypeAst const &lhs_type, asts::ExpressionAst const &access_op);
};


struct spp::analyse::errors::SppCaseBranchMultipleDestructuresError final : spp::utils::errors::SemanticError {
    explicit SppCaseBranchMultipleDestructuresError(asts::CasePatternVariantAst const &first_pattern, asts::CasePatternVariantAst const &second_pattern);
};


struct spp::analyse::errors::SppCaseBranchElseNotLastError final : spp::utils::errors::SemanticError {
    explicit SppCaseBranchElseNotLastError(asts::CaseExpressionBranchAst const &non_last_else_branch, asts::CaseExpressionBranchAst const &last_branch);
};


struct spp::analyse::errors::SppCaseBranchMissingElseError final : spp::utils::errors::SemanticError {
    explicit SppCaseBranchMissingElseError(asts::CaseExpressionAst const &case_expr, asts::CaseExpressionBranchAst const &last_branch);
};


struct spp::analyse::errors::SppIdentifierDuplicateError final : spp::utils::errors::SemanticError {
    explicit SppIdentifierDuplicateError(asts::IdentifierAst const &first_identifier, asts::IdentifierAst const &duplicate_identifier, std::string_view what);
};


struct spp::analyse::errors::SppRecursiveTypeError final : spp::utils::errors::SemanticError {
    explicit SppRecursiveTypeError(asts::ClassPrototypeAst const &type, asts::TypeAst const &recursion);
};


struct spp::analyse::errors::SppCoroutineInvalidReturnTypeError final : spp::utils::errors::SemanticError {
    explicit SppCoroutineInvalidReturnTypeError(asts::CoroutinePrototypeAst const &proto, asts::TypeAst const &return_type);
};


struct spp::analyse::errors::SppNumberOutOfBoundsError final : spp::utils::errors::SemanticError {
    explicit SppNumberOutOfBoundsError(asts::LiteralAst const &literal, boost::multiprecision::cpp_dec_float_100 value, boost::multiprecision::cpp_dec_float_100 lower, boost::multiprecision::cpp_dec_float_100 upper, std::string_view what);
};


struct spp::analyse::errors::SppOrderInvalidError final : spp::utils::errors::SemanticError {
    explicit SppOrderInvalidError(std::string_view first_what, asts::Ast const &first, std::string_view second_what, asts::Ast const &second);
};


struct spp::analyse::errors::SppExpansionOfNonTupleError final : spp::utils::errors::SemanticError {
    explicit SppExpansionOfNonTupleError(asts::Ast const &ast, asts::TypeAst const &type);
};


struct spp::analyse::errors::SppMemoryOverlapUsageError final : spp::utils::errors::SemanticError {
    explicit SppMemoryOverlapUsageError(asts::Ast const &ast, asts::Ast const &overlap_ast);
};


struct spp::analyse::errors::SppMultipleSelfParametersError final : spp::utils::errors::SemanticError {
    explicit SppMultipleSelfParametersError(asts::FunctionParameterSelfAst const &first_self, asts::FunctionParameterSelfAst const &second_self);
};
