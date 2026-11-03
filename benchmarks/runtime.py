#!/usr/bin/env python3
"""Run the run-time benchmarks: each program in S++, C and Rust, and write one JSON record.

Each benchmark is a folder under runtime/<tier>/ (ast, codegen or programs) holding main.spp, main.c and main.rs, and a
manifest.toml. The three programs do the same work and print the same output, which is checked: it proves they agree,
it stops the optimiser deleting the work, and it makes the corpus a test of "-m rel". Each hides its input from the
optimiser (black_box in S++ and Rust, volatile in C), so nothing is folded at compile time.

The C and Rust programs are built to match S++'s checks (the plan's "matched" column): the same baseline x86-64 CPU,
signed overflow trapping in C and overflow checks on in Rust, a stack protector, CET and fortify in C, and panic=abort
in Rust, as S++ aborts.

What the record holds:
 - instructions (with --cachegrind): each binary's, deterministic for one CPU; the S++ one is what the gate reads, and
   the S++/C and S++/Rust ratios of them are the deterministic measure of parity.
 - sizes: the S++ IR before and after optimisation, and each executable's bytes.
 - wall time: the minimum over the rounds, which run the languages interleaved, so a slow stretch on the machine
   affects all three alike. Informational, as are the wall-time ratios.

The manifest says:
  description   what the program does
  layer         "core" (the std code involved is thin: codegen is what is measured) or "std" (std and compiler together)
  output        "exact", or "float" to compare each line as a number within a relative 1e-9
  languages     optional: the ones it is written in, when one has no fair equivalent (default: spp c rust)
  gaps          optional: the known reasons S++ is behind, for the report

  runtime.py [--spp PATH] [--out FILE] [--benchmarks TIER/NAME ...] [--rounds N] [--cachegrind]
"""

import argparse
import hashlib
import json
import math
import shutil
import subprocess
import sys
import tempfile
import time
import tomllib
from pathlib import Path

from run import HERE, ROOT, STD_GIT, cpu_model, llvm_version, log, run, spp_commit, std_mirror, tool_version

TIERS = ("ast", "codegen", "programs")
LANGUAGES = ("spp", "c", "rust")
SOURCES = {"spp": "main.spp", "c": "main.c", "rust": "main.rs"}

C_FLAGS = [
    "-O3", "-march=x86-64", "-ffp-contract=off",
    "-fsanitize=signed-integer-overflow", "-fsanitize-trap=signed-integer-overflow",
    "-fstack-protector-strong", "-fcf-protection=full", "-U_FORTIFY_SOURCE", "-D_FORTIFY_SOURCE=3",
    "-Wl,-z,relro,-z,now",
]  # fmt: skip
RUST_FLAGS = [
    "-C", "opt-level=3", "-C", "target-cpu=x86-64", "-C", "overflow-checks=on", "-C", "panic=abort",
    "-C", "codegen-units=1", "-C", "lto=fat", "--edition=2024",
]  # fmt: skip


def benchmark_names() -> list[str]:
    return sorted(
        f"{tier}/{folder.name}"
        for tier in TIERS
        if (HERE / "runtime" / tier).is_dir()
        for folder in (HERE / "runtime" / tier).iterdir()
        if (folder / "manifest.toml").exists()
    )


def digest(*parts: bytes) -> str:
    h = hashlib.sha256()
    for part in parts:
        h.update(part + b"\0")
    return h.hexdigest()


def build_spp(spp: Path, source: Path, project: Path, mirror: Path, rev: str) -> tuple[Path, dict]:
    if project.exists():
        shutil.rmtree(project)
    (project / "src").mkdir(parents=True)
    shutil.copy(source / "main.spp", project / "src" / "main.spp")
    (project / "spp.toml").write_text(
        f'[project]\nname = "{project.name.replace("-", "_")}"\nversion = "0.1.0"\nbuild = "exe"\n\n'
        f'[vcs]\nstd = {{ git = "{mirror.as_posix()}", rev = "{rev}" }}\n'
    )
    if run([str(spp), "vcs"], cwd=project).returncode != 0:
        sys.exit(f"error: could not check out the std at {rev} for {project.name}")
    stats = project / "stats.json"
    result = run([str(spp), "build", "-m", "rel", "--skip-vcs", f"--emit-stats={stats}"], cwd=project)
    if result.returncode != 0:
        log(result.stdout[-4000:].replace("\r", "\n"))
        sys.exit(f"error: {source.relative_to(HERE)}/main.spp did not build")
    report = json.loads(stats.read_text())
    return project / "out" / report["target"] / "rel" / project.name, report["records"]


def build_native(language: str, source: Path, out: Path, cc: str, rustc: str) -> Path:
    if language == "c":
        command = [cc, *C_FLAGS, "-o", str(out), str(source / "main.c"), "-lm"]
    else:
        command = [rustc, *RUST_FLAGS, "-o", str(out), str(source / "main.rs")]
    result = run(command)
    if result.returncode != 0:
        log(result.stdout[-4000:])
        sys.exit(f"error: {source.relative_to(HERE)}/{SOURCES[language]} did not build")
    return out


def outputs_agree(a: str, b: str, kind: str) -> bool:
    if kind == "exact":
        return a == b
    left, right = a.split(), b.split()
    return len(left) == len(right) and all(
        math.isclose(float(x), float(y), rel_tol=1e-9, abs_tol=1e-12) for x, y in zip(left, right, strict=True)
    )


def instructions(binary: Path, scratch: Path) -> int:
    out = scratch / f"{binary.name}.cachegrind"
    command = ["valgrind", "--tool=cachegrind", "--cache-sim=no", f"--cachegrind-out-file={out}", str(binary)]
    if run(command).returncode != 0:
        sys.exit(f"error: {binary} failed under cachegrind")
    for line in out.read_text().splitlines():
        if line.startswith("summary:"):
            return int(line.split()[1])
    sys.exit(f"error: no summary in {out}")


def measure(name: str, args: argparse.Namespace, spp: Path, mirror: Path, rev: str, work: Path) -> dict:
    source = HERE / "runtime" / name
    manifest = tomllib.loads((source / "manifest.toml").read_text())
    languages = [lang for lang in manifest.get("languages", LANGUAGES) if lang in args.languages]
    kind = manifest.get("output", "exact")
    scratch = work / "runtime" / name.replace("/", "-")
    scratch.mkdir(parents=True, exist_ok=True)

    binaries: dict[str, Path] = {}
    results: dict[str, dict] = {}
    for language in languages:
        text = (source / SOURCES[language]).read_bytes()
        if language == "spp":
            binaries[language], records = build_spp(spp, source, scratch / "spp-project", mirror, rev)
            results[language] = {"hash": digest(text, rev.encode()), "records": records}
        else:
            flags = " ".join(C_FLAGS if language == "c" else RUST_FLAGS)
            binaries[language] = build_native(language, source, scratch / f"main-{language}", args.cc, args.rustc)
            results[language] = {"hash": digest(text, flags.encode())}
        results[language]["executable_bytes"] = binaries[language].stat().st_size

    # Interleaved, so a slow stretch on the machine lands on all of them alike.
    walls: dict[str, list[int]] = {language: [] for language in languages}
    outputs: dict[str, str] = {}
    for _ in range(args.rounds):
        for language in languages:
            start = time.perf_counter_ns()
            result = subprocess.run([str(binaries[language])], capture_output=True, text=True, check=False)
            walls[language].append(time.perf_counter_ns() - start)
            if result.returncode != 0:
                sys.exit(f"error: {name} ({language}) exited {result.returncode}: {result.stderr[-2000:]}")
            outputs.setdefault(language, result.stdout)
            if result.stdout != outputs[language]:
                sys.exit(f"error: {name} ({language}) printed something different from one round to the next")

    reference = outputs[languages[0]]
    for language in languages[1:]:
        if not outputs_agree(outputs[language], reference, kind):
            sys.exit(
                f"error: {name}: {language} printed {outputs[language]!r}, but {languages[0]} printed {reference!r}"
            )

    for language in languages:
        results[language]["wall_ns"] = min(walls[language])
        results[language]["wall_ns_rounds"] = walls[language]
        if args.cachegrind:
            results[language]["instructions"] = instructions(binaries[language], scratch)

    ratios = {}
    for other in ("c", "rust"):
        if "spp" in results and other in results:
            ratios[f"spp/{other}"] = results["spp"]["wall_ns"] / results[other]["wall_ns"]
            if args.cachegrind:
                ratios[f"spp/{other}_instructions"] = results["spp"]["instructions"] / results[other]["instructions"]
    return {
        "description": manifest.get("description", ""),
        "layer": manifest.get("layer", "core"),
        "gaps": manifest.get("gaps", []),
        "output": reference,
        "languages": results,
        "ratios": ratios,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--spp", type=Path, default=ROOT / "cmake-build-release-wsl-dev" / "spp")
    parser.add_argument("--cc", default="clang-23")
    parser.add_argument("--rustc", default=shutil.which("rustc") or str(Path.home() / ".cargo" / "bin" / "rustc"))
    parser.add_argument("--out", type=Path, default=Path("runtime-record.json"))
    parser.add_argument("--benchmarks", nargs="+", default=None, help="default: every one, as tier/name")
    parser.add_argument("--languages", nargs="+", default=list(LANGUAGES), choices=LANGUAGES)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--cachegrind", action="store_true", help="also count each binary's instructions, once")
    parser.add_argument("--std-git", default=STD_GIT, help="where to mirror the std from (a local clone works)")
    parser.add_argument("--work", type=Path, default=None, help="keep the builds here instead of a temporary folder")
    args = parser.parse_args()

    spp = args.spp.resolve()
    if not spp.exists():
        sys.exit(f"error: no spp at {spp}")
    rev = (HERE / "std.rev").read_text().strip()
    names = args.benchmarks or benchmark_names()
    unknown = sorted(set(names) - set(benchmark_names()))
    if unknown:
        sys.exit(f"error: no benchmark {', '.join(unknown)}; there are {', '.join(benchmark_names())}")

    work = args.work.resolve() if args.work else Path(tempfile.mkdtemp(prefix="spp-runtime-"))
    work.mkdir(parents=True, exist_ok=True)
    try:
        mirror = std_mirror(work, args.std_git, rev)
        benchmarks = {}
        for name in names:
            log(f"{name}: {args.rounds} round(s){' and cachegrind' if args.cachegrind else ''}")
            benchmarks[name] = measure(name, args, spp, mirror, rev, work)

        record = {
            "schema": 1,
            "kind": "runtime",
            "keys": {
                "spp_commit": spp_commit(),
                "llvm": llvm_version(spp),
                "std_rev": rev,
                "cc": tool_version([args.cc, "--version"]),
                "rustc": tool_version([args.rustc, "--version"]),
                "cpu": cpu_model(),
                "valgrind": tool_version(["valgrind", "--version"]) if args.cachegrind else None,
                "rounds": args.rounds,
            },
            "benchmarks": benchmarks,
        }
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
        log(f"Wrote {args.out}")
    finally:
        if not args.work:
            shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
