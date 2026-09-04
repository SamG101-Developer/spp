module;
#include <spp/macros.hpp>

module spp.utils.progress;
import spp.utils.types;
import colex;
import sys;

SPP_MOD_BEGIN
spp::utils::ProgressBar::ProgressBar(
  Str label,
  const std::uint32_t total_steps,
  const bool enabled) :
  _Label(std::move(label)),
  _TotalSteps(total_steps),
  _CurrentSteps(0),
  _Enabled(enabled and sys::isatty(sys::fileno(sys::stdout)) != 0),
  _LastDraw() {
}

auto spp::utils::ProgressBar::Next()
  -> void {
  if (not _Enabled) { return; }
  ++_CurrentSteps;

  // Skip some drawing by forcing 30FPS (save on syscalls)
  constexpr auto min_gap = std::chrono::milliseconds(33);
  const auto now = std::chrono::steady_clock::now();
  if (now - _LastDraw < min_gap) { return; }
  _LastDraw = now;

  const auto progress = static_cast<double>(_CurrentSteps) / static_cast<double>(_TotalSteps);
  constexpr auto bar_width = 50;

  const auto halves = static_cast<int>(progress * bar_width * 2.0 + 0.5);
  const auto full = std::min(halves / 2, bar_width);
  const auto half = halves % 2 != 0 and full < bar_width;

  // Fill colour by percentage, fully customizable.
  static const auto fill_cols = OrderedMap<double, Str>{
    {33.3, colex::fg_bright_red + ""},
    {66.6, colex::fg_bright_yellow + ""},
    {100.0, colex::fg_bright_green + ""},
  };

  const auto fill_it = fill_cols.lower_bound(progress * 100.0);
  const auto &fill_col = (fill_it != fill_cols.end() ? fill_it : std::prev(fill_cols.end()))->second;
  static const auto track_col = colex::fg_bright_black + "";
  static const auto reset_col = colex::reset + "";

  // Build the bar into a buffer, then write everything
  // in one shot.
  auto bar = std::string();
  bar.reserve(bar_width * 3 + 64);
  bar += fill_col;
  for (auto i = 0; i < full; ++i) { bar += "━"; }
  if (half) { bar += "╸"; }
  bar += track_col;
  for (auto i = full + (half ? 1 : 0); i < bar_width; ++i) { bar += "─"; }
  bar += reset_col;

  std::printf("\r%-20s %s %s%5.1f%%%s", _Label.c_str(), bar.c_str(), fill_col.c_str(), progress * 100.0, reset_col.c_str());
  std::fflush(sys::stdout);
}

auto spp::utils::ProgressBar::Finish() const
  -> void {
  if (not _Enabled) { return; }

  // Erased rather than left completed on screen, so only one bar is ever visible and the console is clean by the
  // time anything else writes to it - the build's own output, or the program it goes on to run.
  constexpr auto line_width = 90;
  std::printf("\r%*s\r", line_width, "");
  std::fflush(sys::stdout);
}

SPP_MOD_END
