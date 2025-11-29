module;
#include <spp/macros.hpp>

export module spp.analyse.errors.semantic_error_builder;
import spp.analyse.errors.semantic_error;
import spp.utils.errors;
import spp.utils.error_formatter;
import colex;
import genex;
import std;


namespace spp::analyse::errors {
    SPP_EXP_CLS template <typename T> requires std::derived_from<T, SemanticError>
    struct SemanticErrorBuilder;
}


SPP_EXP_CLS template <typename T> requires std::derived_from<T, spp::analyse::errors::SemanticError>
struct spp::analyse::errors::SemanticErrorBuilder final : spp::utils::errors::AbstractErrorBuilder<T> {
    SemanticErrorBuilder() = default;

    SPP_ATTR_NORETURN auto raise() -> void override {
        const auto cast_error = dynamic_cast<SemanticError*>(this->m_err_obj.get());

        // Cycle the formatters to match the number of strings being formatted.
        auto formatters = this->m_error_formatters
            | genex::views::cycle
            | genex::views::take(cast_error->m_error_info.size())
            | genex::to<std::vector>();

        // Format all the error strings by the correct formatter (file agnostic).
        cast_error->messages = cast_error->m_error_info
            | genex::views::zip(std::move(formatters))
            | genex::views::transform([this](auto &&x) { return stringify_error_information(std::get<1>(x), std::get<0>(x)); })
            | genex::to<std::vector>();

        // Throw the error object.
        spp::utils::errors::AbstractErrorBuilder<T>::raise();
    }

private:
    static auto stringify_error_information(
        spp::utils::errors::ErrorFormatter *formatter,
        std::tuple<asts::Ast const*, SemanticError::ErrorInformationType, std::string, std::string> const &info)
        -> std::string {
        using namespace std::string_literals;
        auto [ast, type, tag, msg] = info;

        switch (type) {
        case SemanticError::ErrorInformationType::ERROR: {
            return formatter->error_ast(ast, std::move(msg), std::move(tag));
        }
        case SemanticError::ErrorInformationType::CONTEXT: {
            return formatter->error_ast_minimal(ast, std::move(tag));
        }
        case SemanticError::ErrorInformationType::HEADER: {
            return (colex::fg_bright_white & colex::st_bold) + std::move(msg) + std::move(tag) + "\n"s;
        }
        case SemanticError::ErrorInformationType::FOOTER: {
            return (colex::fg_bright_cyan & colex::st_bold) + std::move(tag) + "\n"s + (colex::fg_bright_red & colex::st_bold) + std::move(msg) + "\n"s;
        }
        default:
            std::unreachable();
        }
    }
};
