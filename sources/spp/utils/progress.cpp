module;
#include <spp/macros.hpp>

module spp.utils.progress;


SPP_MOD_BEGIN


spp::utils::ProgressBar::ProgressBar(
    Str label,
    const std::uint32_t total_steps,
    const bool enabled) :
    _Label(std::move(label)),
    _TotalSteps(total_steps),
    _CurrentSteps(0),
    _Enabled(enabled) {
}


auto spp::utils::ProgressBar::Next()
    -> void {
    if (not _Enabled) { return; }
    ++_CurrentSteps;

    const auto progress = static_cast<float>(_CurrentSteps) / static_cast<float>(_TotalSteps);
    constexpr auto bar_width = 50;
    const auto filled = static_cast<int>(bar_width * progress);

    // Build the bar into a stack buffer, then write everything in one shot.
    char bar[bar_width + 1];
    for (auto i = 0; i < bar_width; ++i)
        bar[i] = i < filled ? '=' : (i == filled ? '>' : ' ');
    bar[bar_width] = '\0';

    std::printf("\r%-20s [%s] %3d%%", _Label.c_str(), bar, static_cast<int>(progress * 100.0f));
    std::fflush(nullptr);
}


auto spp::utils::ProgressBar::Finish() const
    -> void {
    if (not _Enabled) { return; }

    constexpr auto bar_width = 50;
    char bar[bar_width + 1];
    std::fill_n(bar, bar_width, '=');
    bar[bar_width] = '\0';

    std::printf("\r%-20s [%s] 100%%\n", _Label.c_str(), bar);
    std::fflush(nullptr);
}

SPP_MOD_END
