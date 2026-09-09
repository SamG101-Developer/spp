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
  std::atomic<std::uint32_t> _CurrentSteps;
  bool _Enabled;

  // The bar is animated by its own thread, so it keeps
  // gliding towards the step count between calls to
  // "Next", rather than jumping only when work happens
  // to complete.
  std::thread _Renderer;
  std::mutex _Mutex;
  std::condition_variable _Wake;

  bool _Stopped;
  double _Display;
  Str _LastFrame;

  auto Render() -> void;
  auto Draw(double progress) -> void;
  auto DrawIndeterminate(double elapsed) -> void;
  auto Blit(Str const &line) -> void;

public:
  explicit ProgressBar(Str label, std::uint32_t total_steps, bool enabled = true);
  ~ProgressBar();
  ProgressBar(ProgressBar const&) = delete;
  auto operator=(ProgressBar const&) -> ProgressBar& = delete;
  auto Next() -> void;
  auto Finish() -> void;
};
