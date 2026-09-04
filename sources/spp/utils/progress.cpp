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

  const auto progress = static_cast<double>(_CurrentSteps) / static_cast<double>(_TotalSteps);
  constexpr auto bar_width = 50;

  // Todo: Switch to the "colex" library.
  const auto halves = static_cast<int>(progress * bar_width * 2.0 + 0.5);
  const auto full = std::min(halves / 2, bar_width);
  const auto half = halves % 2 != 0 and full < bar_width;

  // Red below a third, yellow below two thirds, green above.
  const auto *fill_col = progress < 1.0 / 3.0 ? "\x1b[38;5;203m" : progress < 2.0 / 3.0 ? "\x1b[38;5;221m" : "\x1b[38;5;77m";
  constexpr auto track_col = "\x1b[38;5;238m";
  constexpr auto reset_col = "\x1b[0m";

  // Build the bar into a buffer, then write everything in one shot. The caps are half-discs, coloured as fill only
  // once the bar has reached them, so the ends read as rounded rather than as extra track.
  auto bar = std::string();
  bar.reserve(bar_width * 3 + 64);
  bar += halves > 0 ? fill_col : track_col;
  bar += fill_col;
  for (auto i = 0; i < full; ++i) { bar += "━"; }
  if (half) { bar += "╸"; }
  bar += track_col;
  for (auto i = full + (half ? 1 : 0); i < bar_width; ++i) { bar += "─"; }
  bar += full == bar_width ? fill_col : track_col;
  bar += reset_col;

  std::printf("\r%-20s %s %s%5.1f%%%s", _Label.c_str(), bar.c_str(), fill_col, progress * 100.0, reset_col);
  std::fflush(nullptr);
}

auto spp::utils::ProgressBar::Finish() const
  -> void {
  if (not _Enabled) { return; }

  // Erased rather than left completed on screen, so only one bar is ever visible and the console is clean by the
  // time anything else writes to it - the build's own output, or the program it goes on to run.
  constexpr auto line_width = 90;
  std::printf("\r%*s\r", line_width, "");
  std::fflush(nullptr);
}

SPP_MOD_END
