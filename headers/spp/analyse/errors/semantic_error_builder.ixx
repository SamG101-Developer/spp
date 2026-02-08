module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
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

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
}

namespace spp {
    SPP_EXP_FUN template <typename... Args>
    auto make_err_args(Args &&... args) -> auto {
        return [&] { return std::make_tuple(std::forward<Args>(args)...); };
    }

    SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError> // and std::constructible_from<analyse::errors::SemanticErrorBuilder<E>, Args...>
    auto raise(std::vector<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
        std::apply(
            [&]<typename... Args2>(Args2 &&... unpacked_args) {
                analyse::errors::SemanticErrorBuilder<E>().with_args(std::forward<Args2>(unpacked_args)...).raises_from_vec(scopes);
            },
            std::forward<A>(arg_binder)());
    }

    SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError> // and std::constructible_from<analyse::errors::SemanticErrorBuilder<E>, Args...>
    auto raise_if(const bool condition, std::vector<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
        if (condition) { raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
    }

    SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError> // and std::constructible_from<analyse::errors::SemanticErrorBuilder<E>, Args...>
    auto raise_unless(const bool condition, std::vector<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
        if (not condition) { raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
    }
}


SPP_EXP_CLS template <typename T> requires std::derived_from<T, spp::analyse::errors::SemanticError>
struct spp::analyse::errors::SemanticErrorBuilder final : spp::utils::errors::AbstractErrorBuilder<T> {
    SemanticErrorBuilder() = default;
    ~SemanticErrorBuilder() override = default;

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
