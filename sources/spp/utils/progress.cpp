module;
#include <spp/macros.hpp>

module spp.utils.progress;
import spp.utils.types;
import colex;
import sys;

SPP_MOD_BEGIN
namespace {
  // Frame pacing and easing. The bar chases the real step
  // count exponentially, so a burst of steps reads as one
  // glide rather than a jump, and a slow stage still moves
  // between steps instead of sitting still.
  constexpr auto kFrameGap = std::chrono::milliseconds(12);
  constexpr auto kStartDelay = std::chrono::milliseconds(90);
  constexpr auto kEaseTau = 0.11;
  constexpr auto kBarWidth = 50;
  constexpr auto kLineWidth = 90;

  // A stage that reports no per-module steps (monomorphisation)
  // gets a sliding segment instead of a bar frozen at 0%.
  constexpr auto kMarqueeWidth = 12;
  constexpr auto kMarqueePeriod = 1.7;
}

spp::utils::ProgressBar::ProgressBar(
  Str label,
  const std::uint32_t total_steps,
  const bool enabled) :
  _Label(std::move(label)),
  _TotalSteps(std::max(total_steps, 1u)),
  _CurrentSteps(0),
  _Enabled(enabled and sys::isatty(sys::fileno(sys::stdout)) != 0),
  _Stopped(false),
  _Display(0.0) {
  if (_Enabled) { _Renderer = std::thread([this] { Render(); }); }
}

spp::utils::ProgressBar::~ProgressBar() {
  // Covers the error paths, where a stage throws before it
  // ever reaches its "Finish". Todo: do we want it to do
  // this?
  Finish();
}

auto spp::utils::ProgressBar::Next()
  -> void {
  // Just bumps the target; the renderer thread owns the terminal.
  _CurrentSteps.fetch_add(1, std::memory_order_relaxed);
}

auto spp::utils::ProgressBar::Render()
  -> void {
  // Nothing is drawn for the first few frames, so stages that
  // finish instantly never flash a part-filled bar.
  auto lock = std::unique_lock(_Mutex);
  if (_Wake.wait_for(lock, kStartDelay, [this] { return _Stopped; })) { return; }

  const auto start = std::chrono::steady_clock::now();
  auto last = start;
  while (not _Stopped) {

    // Next frame - get the current time and the delta time,
    // to work out the next render.
    const auto now = std::chrono::steady_clock::now();
    const auto dt = std::chrono::duration<double>(now - last).count();
    last = now;

    // Not stepping means this is an indeterminate line, so
    // call that instead.
    const auto steps = _CurrentSteps.load(std::memory_order_relaxed);
    if (steps == 0) {
      DrawIndeterminate(std::chrono::duration<double>(now - start).count());
    }

    // Otherwise do the normal progress render instead, adding
    // the next chunk into the line.
    else {
      const auto target = static_cast<double>(steps) / static_cast<double>(_TotalSteps);
      _Display += (target - _Display) * (1.0 - std::exp(-dt / kEaseTau));
      if (std::abs(target - _Display) < 0.0005) { _Display = target; }
      Draw(std::min(_Display, 1.0));
    }

    _Wake.wait_for(lock, kFrameGap, [this] { return _Stopped; });
  }
}

auto spp::utils::ProgressBar::Draw(
  const double progress)
  -> void {
  const auto halves = static_cast<int>(progress * kBarWidth * 2.0 + 0.5);
  const auto full = std::min(halves / 2, kBarWidth);
  const auto half = halves % 2 != 0 and full < kBarWidth;

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

  // Build the whole line into a buffer, so it can be
  // compared against the last one and written in one
  // shot.
  auto line = std::string();
  line.reserve(kBarWidth * 3 + 96);
  line += std::format("{:<20} ", _Label.c_str());
  line += fill_col;
  for (auto i = 0; i < full; ++i) { line += "━"; }
  if (half) { line += "╸"; }
  line += track_col;
  for (auto i = full + (half ? 1 : 0); i < kBarWidth; ++i) { line += "─"; }
  line += reset_col;
  line += std::format(" {}{:5.1f}%{}", fill_col, progress * 100.0, reset_col);
  Blit(line);
}

auto spp::utils::ProgressBar::DrawIndeterminate(
  const double elapsed)
  -> void {
  // Sinusoidal, so the segment eases into each turn
  // rather than bouncing off the ends.
  static const auto fill_col = colex::fg_bright_cyan + "";
  static const auto track_col = colex::fg_bright_black + "";
  static const auto reset_col = colex::reset + "";

  const auto phase = (1.0 - std::cos(2.0 * std::numbers::pi * elapsed / kMarqueePeriod)) / 2.0;
  const auto head = static_cast<int>(phase * (kBarWidth - kMarqueeWidth) + 0.5);

  auto line = std::string();
  line.reserve(kBarWidth * 3 + 96);
  line += std::format("{:<20} ", _Label.c_str());
  line += track_col;
  for (auto i = 0; i < head; ++i) { line += "━"; }
  line += fill_col;
  for (auto i = 0; i < kMarqueeWidth; ++i) { line += "━"; }
  line += track_col;
  for (auto i = head + kMarqueeWidth; i < kBarWidth; ++i) { line += "━"; }
  line += reset_col;
  line += Str(7, ' ');
  Blit(line);
}

auto spp::utils::ProgressBar::Blit(
  Str const &line)
  -> void {
  // An unchanged frame is not worth a syscall.
  if (line == _LastFrame) { return; }
  _LastFrame = line;
  std::fwrite("\r", 1, 1, sys::stdout);
  std::fwrite(line.data(), 1, line.size(), sys::stdout);
  std::fflush(sys::stdout);
}

auto spp::utils::ProgressBar::Finish()
  -> void {
  {
    auto lock = std::lock_guard(_Mutex);
    if (_Stopped) { return; }
    _Stopped = true;
  }
  _Wake.notify_all();
  if (_Renderer.joinable()) { _Renderer.join(); }
  if (not _Enabled or _LastFrame.empty()) { return; }

  // Erased rather than left completed on screen, so only
  // one bar is ever visible and the console is clean by the
  // time anything else writes to it - the build's own
  // output, or the program it goes on to run.
  std::printf("\r%*s\r", kLineWidth, "");
  std::fflush(sys::stdout);
}

SPP_MOD_END
