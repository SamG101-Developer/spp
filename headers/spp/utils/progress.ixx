module;
#include <spp/macros.hpp>

export module spp.utils.progress;
import spp.utils.types;
import std;

namespace spp::utils {
    SPP_EXP_CLS class ProgressBar;
}

SPP_EXP_CLS class spp::utils::ProgressBar {
    Str _Label;
    std::uint32_t _TotalSteps;
    std::uint32_t _CurrentSteps;
    bool _Enabled;

public:
    explicit ProgressBar(Str label, std::uint32_t total_steps, bool enabled = true);
    auto Next() -> void;
    auto Finish() const -> void;
};
