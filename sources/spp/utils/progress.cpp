module;
#include <spp/macros.hpp>

module spp.utils.progress;


SPP_MOD_BEGIN


spp::utils::ProgressBar::ProgressBar(
    std::string label,
    const std::uint32_t total_steps,
    const bool enabled) :
    m_label(std::move(label)),
    m_total_steps(total_steps),
    m_current_step(0),
    m_enabled(enabled) {
}


auto spp::utils::ProgressBar::next()
    -> void {
    if (not m_enabled) { return; }
    ++m_current_step;

    const auto progress = static_cast<float>(m_current_step) / static_cast<float>(m_total_steps);
    constexpr auto bar_width = 50;
    const auto filled = static_cast<int>(bar_width * progress);

    // Build the bar into a stack buffer, then write everything in one shot.
    char bar[bar_width + 1];
    for (auto i = 0; i < bar_width; ++i)
        bar[i] = i < filled ? '=' : (i == filled ? '>' : ' ');
    bar[bar_width] = '\0';

    std::printf("\r%-20s [%s] %3d%%", m_label.c_str(), bar, static_cast<int>(progress * 100.0f));
    std::fflush(nullptr);
}


auto spp::utils::ProgressBar::finish() const
    -> void {
    if (not m_enabled) { return; }

    constexpr auto bar_width = 50;
    char bar[bar_width + 1];
    std::fill_n(bar, bar_width, '=');
    bar[bar_width] = '\0';

    std::printf("\r%-20s [%s] 100%%\n", m_label.c_str(), bar);
    std::fflush(nullptr);
}

SPP_MOD_END
