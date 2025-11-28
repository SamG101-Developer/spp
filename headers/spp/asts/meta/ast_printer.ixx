module;
#include <spp/macros.hpp>

export module spp.asts.meta.ast_printer;
import std;
import spp.asts._fwd;


template <typename, template<typename...> class>
struct is_same_template : std::false_type {
};

template <template<typename...> class C, typename... Args>
struct is_same_template<C<Args...>, C> : std::true_type {
};

template <typename T, template <typename...> class C>
inline constexpr auto is_same_template_v = is_same_template<T, C>::value;


namespace spp::asts::meta {
    SPP_EXP_CLS class AstPrinter;
}


SPP_EXP_CLS class spp::asts::meta::AstPrinter {
    std::uint32_t m_indent_size;
    std::uint32_t m_indent_level;

    auto m_increase_indent() -> void;
    auto m_decrease_indent() -> void;
    auto m_format_code(std::string &&code) const -> std::string;

public:
    explicit AstPrinter(std::uint32_t indent_size = 4);

    template <typename T>
    auto print(T &&ast) -> std::string;
};


template <typename T>
auto spp::asts::meta::AstPrinter::print(T &&ast) -> std::string {
    constexpr auto do_indent =
        is_same_template_v<T, InnerScopeAst> or
        std::is_same_v<T, FunctionImplementationAst> or
        std::is_same_v<T, ClassImplementationAst> or
        std::is_same_v<T, SupImplementationAst>;

    auto code = std::string();
    if (do_indent) {
        m_increase_indent();
        code = ast.print(*this);
        code = m_format_code(std::move(code));
        m_decrease_indent();
    }
    else {
        code = ast.print(*this);
        code = m_format_code(std::move(code));
    }

    return code;
}
