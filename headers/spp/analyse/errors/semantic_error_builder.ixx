module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.errors.semantic_error;
import spp.utils.errors;
import spp.utils.error_formatter;
import spp.utils.types;
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
    auto MakeErrArgs(Args &&... args) -> auto {
        return [&] { return std::make_tuple(std::forward<Args>(args)...); };
    }

    SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError>
    SPP_ATTR_NORETURN auto Raise(Vec<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
        std::apply(
            [&]<typename... Args2>(Args2 &&... unpacked_args) {
                analyse::errors::SemanticErrorBuilder<E>().WithArgs(std::forward<Args2>(unpacked_args)...).raises_from_vec(scopes);
            },
            std::forward<A>(arg_binder)());
        std::unreachable();
    }

    SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError>
    auto RaiseIf(const bool condition, Vec<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
        if (condition) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
    }

    SPP_EXP_FUN template <typename E, typename A, typename F, typename V>
    requires std::derived_from<E, analyse::errors::SemanticError>
    auto RaiseIfAny(F &&condition, V const &vector, Vec<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
        for (auto const &v : vector) {
            if (condition(v)) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
        }
    }

    SPP_EXP_FUN template <typename E, typename A>
    requires std::derived_from<E, analyse::errors::SemanticError>
    auto RaiseUnless(const bool condition, Vec<analyse::scopes::Scope const*> const &scopes, A &&arg_binder) -> void {
        if (not condition) { Raise<E>(std::move(scopes), std::forward<A>(arg_binder)); }
    }
}

SPP_EXP_CLS template <typename T> requires std::derived_from<T, spp::analyse::errors::SemanticError>
struct spp::analyse::errors::SemanticErrorBuilder final : spp::utils::errors::AbstractErrorBuilder<T> {
    SemanticErrorBuilder() = default;
    ~SemanticErrorBuilder() override = default;

    SPP_ATTR_NORETURN auto Raise() -> void override {
        const auto cast_error = dynamic_cast<SemanticError*>(this->_ErrObj.get());

        // Cycle the formatters to match the number of strings being formatted.
        auto formatters = this->_ErrFormatters
            | genex::views::cycle
            | genex::views::take(cast_error->m_error_info.Len())
            | genex::to<Vec>();

        // Format all the error strings by the correct formatter (file agnostic).
        cast_error->messages = cast_error->m_error_info
            | genex::views::zip(std::move(formatters))
            | genex::views::transform([this](auto &&x) { return _StringifyErrorInformation(std::get<1>(x), std::get<0>(x)); })
            | genex::to<Vec>();

        // Throw the error object.
        spp::utils::errors::AbstractErrorBuilder<T>::Raise();
    }

private:
    static auto _StringifyErrorInformation(
        spp::utils::errors::ErrorFormatter *formatter,
        std::tuple<asts::Ast const*, SemanticError::ErrorInformationType, Str, Str> const &info)
        -> Str {
        using namespace std::string_literals;
        auto [ast, type, tag, msg] = info;

        switch (type) {
        case SemanticError::ErrorInformationType::ERROR: {
            return formatter->ErrorAst(ast, std::move(msg), std::move(tag));
        }
        case SemanticError::ErrorInformationType::CONTEXT: {
            return formatter->ErrorAstMinimal(ast, std::move(tag));
        }
        case SemanticError::ErrorInformationType::HEADER: {
            return (colex::fg_bright_white & colex::st_bold) + std::move(msg) + ": "s + std::move(tag) + "\n"s;
        }
        case SemanticError::ErrorInformationType::FOOTER: {
            return (colex::fg_bright_cyan & colex::st_bold) + std::move(tag) + "\n"s + (colex::fg_bright_red & colex::st_bold) + std::move(msg) + "\n"s;
        }
        default:
            std::unreachable();
        }
    }
};
