module;
#include <spp/macros.hpp>

export module spp.utils.progress;
import std;


namespace spp::utils {
    SPP_EXP_CLS class ProgressBar;
}


SPP_EXP_CLS class spp::utils::ProgressBar {
    std::string m_label;
    std::uint32_t m_total_steps;
    std::uint32_t m_current_step;

public:
    explicit ProgressBar(std::string label, std::uint32_t total_steps);
    auto next() -> void;
    auto finish() -> void;
};
