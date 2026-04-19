module spp.utils.progress;


spp::utils::ProgressBar::ProgressBar(
    std::string label,
    const std::uint32_t total_steps,
    const bool enabled) :
    m_label(std::move(label)),
    m_total_steps(total_steps),
    m_current_step(0),
    m_enabled(enabled) {
}


auto spp::utils::ProgressBar::next() -> void {
    // Use "\r" to return to the beginning of the line
    if (not m_enabled) { return; }
    ++m_current_step;
    const auto progress = static_cast<float>(m_current_step) / static_cast<float>(m_total_steps);
    constexpr auto bar_width = 100;

    std::cout << "\r" << m_label << " [";
    const auto pos = static_cast<int>(bar_width * progress);
    for (auto i = 0; i < bar_width; ++i) {
        if (i < pos) {
            std::cout << "=";
        }
        else if (i == pos) {
            std::cout << ">";
        }
        else {
            std::cout << " ";
        }
    }

    std::cout << "] " << static_cast<int>(progress * 100.0f) << " %";
    std::cout.flush();
}


auto spp::utils::ProgressBar::finish() -> void {
    if (not m_enabled) { return; }
    m_current_step = m_total_steps;
    next();
    std::cout << std::endl;
}
