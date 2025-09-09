#include <spp/asts/meta/ast_printer.hpp>
#include <spp/pch.hpp>


spp::asts::meta::AstPrinter::AstPrinter(
    const std::uint32_t indent_size) :
    m_indent_size(indent_size),
    m_indent_level(0) {
}


auto spp::asts::meta::AstPrinter::m_increase_indent() -> void {
    m_indent_level += m_indent_size;
}


auto spp::asts::meta::AstPrinter::m_decrease_indent() -> void {
    m_indent_level -= m_indent_size;
}


auto spp::asts::meta::AstPrinter::m_format_code(std::string &&code) const -> std::string {
    if (m_indent_level == 0) {
        return code;
    }

    auto lines = std::vector<std::string>();
    auto start = 0uz, end = 0uz;

    while ((end = code.find('\n', start)) != std::string::npos) {
        lines.push_back(code.substr(start, end));
        start = end + 1;
    }

    if (start <= code.length()) {
        lines.push_back(code.substr(start));
    }

    const auto indent = std::string(m_indent_level, ' ');
    for (auto &line : lines) {
        line = indent + line;
    }

    auto formatted = std::string();
    for (const auto &line : lines) {
        formatted += line + '\n';
    }

    return formatted;
}
