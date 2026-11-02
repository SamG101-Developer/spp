#!/usr/bin/env python3
"""Compare a benchmark record (run.py or runtime.py) against a baseline, and fail on a regression.

A run-time record is judged on its S++ programs only (instructions and sizes, as below), and always ends with the
parity table: S++ against C and Rust, by layer, never judged.

Each metric is judged by what kind of number it is:
 - counters: the work the compiler did. Machine-independent, so any increase is a regression (tolerance 0 by default).
   The counters of hits ("..._cached", "..._by_stamp") are shown but never judged, as more of them is better.
 - sizes ("records": IR before and after optimisation, object and executable bytes): judged at --size-tolerance.
 - instructions (cachegrind): judged at --instruction-tolerance, only between records with the same CPU model, as glibc
   picks its string functions by CPU feature.
 - wall time and peak RSS: shown, never judged. A shared runner's timings are too noisy for one run to decide anything.

A metric is only compared when nothing it depends on differs: a workload whose hash differs (its source or the std
rev changed) is not compared at all, and a different LLVM skips the sizes and instructions. Each skip is reported with
its reason. With no baseline, the record is reported and nothing fails.

A counter regression names the phase where most of the increase happened, as "+4%" alone says nothing about where to
look.

  compare.py --record FILE [--baseline FILE] [--summary FILE] [--report-only]
"""

import argparse
import json
import os
import sys
from pathlib import Path

HIT_COUNTERS = ("_cached", "_by_stamp")


def annotate(level: str, message: str) -> None:
    print(f"::{level}::{message}" if os.environ.get("GITHUB_ACTIONS") else f"{level}: {message}")


def percent(head: float, base: float) -> str:
    if base == 0:
        return "new" if head else "0%"
    return f"{(head - base) / base * 100:+.2f}%"


def worst_phase(counter: str, head: dict, base: dict) -> str:
    base_phases = {p["name"]: p["counters"].get(counter, 0) for p in base["phases"]}
    deltas = [(p["counters"].get(counter, 0) - base_phases.get(p["name"], 0), p["name"]) for p in head["phases"]]
    delta, name = max(deltas, default=(0, ""))
    return f"mostly in {name} (+{delta:,})" if delta > 0 else ""


class Comparison:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.rows: list[tuple[str, str, str, str, str, str]] = []
        self.regressions = 0
        self.improvements = 0
        self.skipped: list[str] = []

    def judge(self, where: str, metric: str, head: float, base: float, tolerance: float | None, note: str = "") -> None:
        if tolerance is None:
            status = "info"
        elif head > base * (1 + tolerance):
            status = "REGRESSED"
            self.regressions += 1
        elif head < base:
            status = "improved"
            self.improvements += 1
        else:
            status = "ok"
        if status in ("ok",) or (status == "info" and head == base):
            return
        self.rows.append(
            (where, metric, f"{base:,.0f}", f"{head:,.0f}", percent(head, base), f"{status} {note}".strip())
        )

    def mode(self, where: str, head: dict, base: dict, same_llvm: bool, same_cpu: bool) -> None:
        if not head["deterministic"] or not base["deterministic"]:
            self.skipped.append(f"{where}: counters differ between rounds, so they are not compared")
        else:
            for counter, value in head["counters"].items():
                if counter not in base["counters"]:
                    continue
                hit = counter.endswith(HIT_COUNTERS)
                tolerance = None if hit else self.args.counter_tolerance
                note = worst_phase(counter, head, base) if not hit and value > base["counters"][counter] else ""
                self.judge(where, counter, value, base["counters"][counter], tolerance, note)

        if not same_llvm:
            self.skipped.append(f"{where}: sizes and instructions not compared, as the LLVM differs")
        else:
            for record, value in head["records"].items():
                if record in base["records"]:
                    self.judge(where, record, value, base["records"][record], self.args.size_tolerance)
            if "instructions" in head and "instructions" in base:
                if same_cpu:
                    self.judge(
                        where,
                        "instructions",
                        head["instructions"],
                        base["instructions"],
                        self.args.instruction_tolerance,
                    )
                else:
                    self.skipped.append(f"{where}: instructions not compared, as the CPU differs")

        self.judge(where, "wall_ns", head["wall_ns"], base["wall_ns"], None)
        self.judge(where, "peak_rss_bytes", head["peak_rss_bytes"], base["peak_rss_bytes"], None)

    def runtime(self, record: dict, baseline: dict, same_llvm: bool, same_cpu: bool) -> None:
        """Only the S++ program is judged: C and Rust are the yardstick, not the subject."""
        for name, benchmark in record["benchmarks"].items():
            base = baseline["benchmarks"].get(name)
            if base is None or "spp" not in base["languages"]:
                self.skipped.append(f"{name}: new, so there is no baseline for it")
                continue
            head, previous = benchmark["languages"]["spp"], base["languages"]["spp"]
            if head["hash"] != previous["hash"]:
                self.skipped.append(f"{name}: its S++ source or the std rev changed, so it is not compared")
                continue
            if not same_llvm:
                self.skipped.append(f"{name}: sizes and instructions not compared, as the LLVM differs")
            else:
                for size, value in head["records"].items():
                    if size in previous["records"]:
                        self.judge(name, size, value, previous["records"][size], self.args.size_tolerance)
                self.judge(
                    name, "executable_bytes", head["executable_bytes"], previous["executable_bytes"],
                    self.args.size_tolerance,
                )  # fmt: skip
                if "instructions" in head and "instructions" in previous:
                    if same_cpu:
                        self.judge(
                            name, "instructions", head["instructions"], previous["instructions"],
                            self.args.instruction_tolerance,
                        )  # fmt: skip
                    else:
                        self.skipped.append(f"{name}: instructions not compared, as the CPU differs")
            self.judge(name, "wall_ns", head["wall_ns"], previous["wall_ns"], None)

    def run(self, record: dict, baseline: dict) -> None:
        keys, base_keys = record["keys"], baseline["keys"]
        same_llvm = keys["llvm"] == base_keys["llvm"]
        same_cpu = keys["cpu"] == base_keys["cpu"] and keys.get("valgrind") == base_keys.get("valgrind")
        if record.get("kind") == "runtime":
            self.runtime(record, baseline, same_llvm, same_cpu)
            return
        for name, workload in record["workloads"].items():
            base = baseline["workloads"].get(name)
            if base is None:
                self.skipped.append(f"{name}: new, so there is no baseline for it")
                continue
            if workload["hash"] != base["hash"]:
                self.skipped.append(f"{name}: its source or the std rev changed, so it is not compared")
                continue
            for mode, head in workload["modes"].items():
                if mode in base["modes"]:
                    self.mode(f"{name} ({mode})", head, base["modes"][mode], same_llvm, same_cpu)


def parity(record: dict) -> list[str]:
    """S++ against C and Rust, per layer: never judged, as S++ is expected to be behind for now."""

    def ratio(benchmark: dict, key: str) -> str:
        value = benchmark["ratios"].get(key)
        return f"{value:.2f}x" if value is not None else "-"

    lines = []
    for layer in ("core", "std"):
        rows = [(name, b) for name, b in record["benchmarks"].items() if b["layer"] == layer]
        if not rows:
            continue
        lines += [
            "",
            f"Parity, {layer} layer (S++ time over C's and Rust's; instructions likewise):",
            "",
            "| benchmark | S++ | vs C | vs Rust | instr. vs C | instr. vs Rust | known gaps |",
            "|---|---|---|---|---|---|---|",
        ]
        for name, b in rows:
            lines.append(
                f"| {name} | {b['languages']['spp']['wall_ns'] / 1e6:.1f} ms | {ratio(b, 'spp/c')} "
                f"| {ratio(b, 'spp/rust')} | {ratio(b, 'spp/c_instructions')} | {ratio(b, 'spp/rust_instructions')} "
                f"| {'; '.join(b['gaps'])} |"
            )
    return lines


def report_only(record: dict) -> list[str]:
    if record.get("kind") == "runtime":
        return []
    lines = ["| workload | wall | peak RSS | AST nodes | instructions |", "|---|---|---|---|---|"]
    for name, workload in record["workloads"].items():
        for mode, d in workload["modes"].items():
            instructions = f"{d['instructions']:,}" if "instructions" in d else "-"
            lines.append(
                f"| {name} ({mode}) | {d['wall_ns'] / 1e9:.2f} s | {d['peak_rss_bytes'] / 2**20:.0f} MiB "
                f"| {d['counters']['ast_nodes_created']:,} | {instructions} |"
            )
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--record", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, default=None)
    parser.add_argument(
        "--summary", type=Path, default=None, help="also write the markdown here (default: the step summary)"
    )
    parser.add_argument("--report-only", action="store_true", help="report regressions, but do not fail")
    parser.add_argument("--counter-tolerance", type=float, default=0.0)
    parser.add_argument("--size-tolerance", type=float, default=0.01)
    parser.add_argument("--instruction-tolerance", type=float, default=0.005)
    args = parser.parse_args()

    record = json.loads(args.record.read_text())
    runtime = record.get("kind") == "runtime"
    lines = ["## Run-time benchmarks" if runtime else "## Compile-time benchmarks", ""]
    status = 0

    if args.baseline is None or not args.baseline.exists():
        annotate("warning", "There is no baseline yet, so nothing has been compared.")
        lines += ["No baseline yet, so nothing was compared. This run:", "", *report_only(record)]
    else:
        comparison = Comparison(args)
        comparison.run(record, json.loads(args.baseline.read_text()))
        lines.append(
            f"{comparison.regressions} regression(s), {comparison.improvements} improvement(s) against the baseline "
            f"for std {record['keys']['std_rev'][:12]}."
        )
        if comparison.improvements:
            lines.append("The baseline tightens to the improved values when this merges.")
        if comparison.rows:
            order = {"REGRESSED": 0, "improved": 1, "info": 2}
            rows = sorted(comparison.rows, key=lambda row: order.get(row[5].split()[0], 3))
            lines += ["", "| workload | metric | baseline | this run | change | |", "|---|---|---|---|---|---|"]
            lines += [f"| {' | '.join(row)} |" for row in rows]
        if comparison.skipped:
            lines += ["", "Not compared:", ""] + [f"- {reason}" for reason in comparison.skipped]
        for skipped in comparison.skipped:
            annotate("warning", skipped)
        if comparison.regressions:
            what = "run-time" if runtime else "compile-time"
            annotate("error", f"{comparison.regressions} {what} regression(s); see the summary.")
            status = 0 if args.report_only else 1

    if runtime:
        lines += parity(record)
    text = "\n".join(lines) + "\n"
    print(text)
    summary = args.summary or (Path(os.environ["GITHUB_STEP_SUMMARY"]) if "GITHUB_STEP_SUMMARY" in os.environ else None)
    if summary:
        with summary.open("a", encoding="utf-8") as file:
            file.write(text)
    return status


if __name__ == "__main__":
    sys.exit(main())
