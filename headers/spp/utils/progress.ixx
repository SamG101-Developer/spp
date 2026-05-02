module;
#include <spp/macros.hpp>

export module spp.utils.progress;
import spp.utils.types;
import std;


namespace spp::utils {
    SPP_EXP_CLS class ProgressBar;
}


SPP_EXP_CLS class spp::utils::ProgressBar {
    Str m_label;
    std::uint32_t m_total_steps;
    std::uint32_t m_current_step;
    bool m_enabled;

public:
    explicit ProgressBar(Str label, std::uint32_t total_steps, bool enabled = true);
    auto next() -> void;
    auto finish() const -> void;
};
