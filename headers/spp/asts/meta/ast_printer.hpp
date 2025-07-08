#ifndef AST_PRINTER_HPP
#define AST_PRINTER_HPP

#include <cstdint>

#include <unicode/unistr.h>


#define SPP_PRINT_START auto formatted_string = icu::UnicodeString()
#define SPP_PRINT_APPEND(x) formatted_string.append(x->print(printer))
#define SPP_PRINT_EXTEND(x) std::ranges::for_each(x, [&formatted_string, &printer](auto &&elem) { formatted_string.append(elem->print(printer)); })
#define SPP_PRINT_END return formatted_string

#define SPP_STRING_START auto raw_string = icu::UnicodeString()
#define SPP_STRING_APPEND(x) raw_string.append(static_cast<icu::UnicodeString>(*x))
#define SPP_STRING_EXTEND(x) std::ranges::for_each(x, [&raw_string, this](auto &&elem) { raw_string.append(static_cast<icu::UnicodeString>(*elem)); })
#define SPP_STRING_END return raw_string


template <typename, template<typename...> class>
struct is_same_template : std::false_type {
};

template <template<typename...> class C, typename... Args>
struct is_same_template<C<Args...>, C> : std::true_type {
};

template <typename T, template <typename...> class C>
inline constexpr auto is_same_template_v = is_same_template<T, C>::value;


namespace spp::asts {
    template <typename T>
    struct InnerScopeAst;
    struct FunctionImplementationAst;
    struct ClassImplementationAst;
    struct SupImplementationAst;
}

namespace spp::asts::meta {
    class AstPrinter;
}


class spp::asts::meta::AstPrinter {
public:
    explicit AstPrinter(std::uint32_t indent_size = 4);

    template <typename T>
    auto print(T &&ast) -> icu::UnicodeString;

private:
    auto m_increase_indent() -> void;
    auto m_decrease_indent() -> void;
    auto m_format_code(icu::UnicodeString &&code) const -> icu::UnicodeString;

private:
    std::uint32_t m_indent_size;
    std::uint32_t m_indent_level;
};


template <typename T>
auto spp::asts::meta::AstPrinter::print(T &&ast) -> icu::UnicodeString {
    constexpr auto do_indent =
        is_same_template_v<T, InnerScopeAst> ||
        std::is_same_v<T, FunctionImplementationAst> ||
        std::is_same_v<T, ClassImplementationAst> ||
        std::is_same_v<T, SupImplementationAst>;

    auto code = icu::UnicodeString();
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


#endif //AST_PRINTER_HPP
