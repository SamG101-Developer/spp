#!/usr/bin/env python3
"""Run the compile-time benchmarks and write one JSON record of the results.

Each workload under compile/ is either a fixed project (its src/) or one written by its generate.py. Every workload is
built against the std commit in std.rev, through a local mirror of the std's repository made once per run, so nothing
but the first clone touches the network and every workload reads the same code. Each is built at each mode for a
number of rounds with "spp build --emit-stats", and optionally once more under cachegrind for its instruction count.

What the record holds, and which of it can be trusted to compare:
 - counters, and the IR and binary sizes: machine-independent, so exactly comparable. A workload whose counters differ
   between rounds is marked non-deterministic, as that breaks the assumption the comparison rests on.
 - instructions: deterministic for one CPU, but glibc picks its string functions by CPU feature, so only comparable
   between records with the same CPU model.
 - wall time and peak RSS: the minimum over the rounds, which is the least noisy estimate; only ever informational.

The record's "keys" say what it was made with. compare.py refuses to compare what a differing key makes incomparable.

  run.py [--spp PATH] [--out FILE] [--workloads NAME ...] [--modes dev rel] [--rounds N] [--cachegrind]
"""

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
STD_GIT = "https://github.com/SamG101-Developer/SPP-STL"


def log(message: str) -> None:
    print(message, file=sys.stderr, flush=True)


def run(command: list[str], cwd: Path | None = None, quiet: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        command,
        cwd=cwd,
        text=True,
        check=False,
        stdout=subprocess.PIPE if quiet else None,
        stderr=subprocess.STDOUT if quiet else None,
    )


def std_mirror(work: Path, std_git: str, rev: str) -> Path:
    """A mirror of the std's repository, holding every branch, so a pin on any of them can be checked out from it."""
    mirror = work / "std.git"
    if not mirror.exists():
        log(f"Mirroring the std from {std_git}")
        if run(["git", "clone", "--quiet", "--mirror", std_git, str(mirror)]).returncode != 0:
            sys.exit(f"error: could not mirror the std from {std_git}")
    if run(["git", "-C", str(mirror), "cat-file", "-e", f"{rev}^{{commit}}"]).returncode != 0:
        sys.exit(f"error: the std has no commit {rev}; is std.rev a commit on one of its branches?")
    return mirror


def workload_names() -> list[str]:
    return sorted(p.name for p in (HERE / "compile").iterdir() if p.is_dir())


def make_project(name: str, project: Path, mirror: Path, rev: str) -> str:
    """Write the workload's project, and answer the hash of everything in it that the compile reads."""
    source = HERE / "compile" / name
    if project.exists():
        shutil.rmtree(project)
    project.mkdir(parents=True)
    if (source / "generate.py").exists():
        if run([sys.executable, str(source / "generate.py"), str(project)]).returncode != 0:
            sys.exit(f"error: {name}'s generate.py failed")
    else:
        shutil.copytree(source / "src", project / "src")

    (project / "spp.toml").write_text(
        f'[project]\nname = "{name.replace("-", "_")}"\nversion = "0.1.0"\nbuild = "exe"\n\n'
        f'[vcs]\nstd = {{ git = "{mirror.as_posix()}", rev = "{rev}" }}\n'
    )

    digest = hashlib.sha256()
    for file in sorted((project / "src").rglob("*.spp")):
        digest.update(file.relative_to(project).as_posix().encode() + b"\0" + file.read_bytes() + b"\0")
    digest.update(rev.encode())
    return digest.hexdigest()


def build(spp: Path, project: Path, mode: str, stats: Path, wrapper: list[str] | None = None) -> None:
    command = (wrapper or []) + [str(spp), "build", "-m", mode, "--skip-vcs", f"--emit-stats={stats}"]
    result = run(command, cwd=project)
    if result.returncode != 0:
        log(result.stdout[-4000:].replace("\r", "\n"))
        how = f"killed by signal {-result.returncode}" if result.returncode < 0 else f"exit {result.returncode}"
        sys.exit(f"error: {project.name} did not build at {mode} ({how})")


def instructions(spp: Path, project: Path, mode: str, scratch: Path) -> int:
    """Instructions the compiler retired building the workload. The linker is a child process, and not counted."""
    out = scratch / f"cachegrind-{mode}.out"
    build(
        spp,
        project,
        mode,
        scratch / f"cachegrind-{mode}.json",
        ["valgrind", "--tool=cachegrind", "--cache-sim=no", f"--cachegrind-out-file={out}"],
    )
    for line in out.read_text().splitlines():
        if line.startswith("summary:"):
            return int(line.split()[1])
    sys.exit(f"error: no summary in {out}")


def measure(spp: Path, project: Path, mode: str, rounds: int, cachegrind: bool) -> dict:
    scratch = project / "bench"
    scratch.mkdir(exist_ok=True)
    reports = []
    for i in range(rounds):
        stats = scratch / f"{mode}-{i}.json"
        build(spp, project, mode, stats)
        reports.append(json.loads(stats.read_text()))

    first = reports[0]
    phases = [
        {
            "name": phase["name"],
            "wall_ns": min(r["phases"][j]["wall_ns"] for r in reports),
            "counters": phase["counters"],
        }
        for j, phase in enumerate(first["phases"])
    ]
    result = {
        "counters": first["total"]["counters"],
        "records": first["records"],
        "deterministic": all(
            r["total"]["counters"] == first["total"]["counters"] and r["records"] == first["records"] for r in reports
        ),
        "wall_ns": min(r["total"]["wall_ns"] for r in reports),
        "wall_ns_rounds": [r["total"]["wall_ns"] for r in reports],
        "peak_rss_bytes": min(r["total"]["peak_rss_bytes"] for r in reports),
        "phases": phases,
        "spp": first["spp"],
        "target": first["target"],
    }
    if cachegrind:
        result["instructions"] = instructions(spp, project, mode, scratch)
    return result


def cpu_model() -> str:
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def llvm_version(spp: Path) -> str:
    """The LLVM built into the binary, from "spp version"; or, for a binary older than that line, the CMake cache."""
    match = re.search(r"^LLVM version (\S+)$", run([str(spp), "version"]).stdout, re.M)
    if match:
        return match.group(1)
    cache = spp.parent / "CMakeCache.txt"
    if cache.exists():
        # LLVM_DIR is "<prefix>/lib/cmake/llvm" in LLVM's own layout and "<prefix>/cmake" in Debian's.
        match = re.search(r"^LLVM_DIR:PATH=(.+)$", cache.read_text(), re.M)
        if match:
            for prefix in Path(match.group(1)).parents:
                config = prefix / "bin" / "llvm-config"
                if config.exists():
                    return run([str(config), "--version"]).stdout.strip()
    return "unknown"


def spp_commit() -> str:
    head = run(["git", "-C", str(ROOT), "rev-parse", "HEAD"]).stdout.strip() or "unknown"
    dirty = run(["git", "-C", str(ROOT), "diff", "--quiet", "HEAD", "--", "headers", "sources"]).returncode != 0
    return head + ("-dirty" if dirty else "")


def tool_version(command: list[str]) -> str:
    try:
        output = run(command).stdout.strip()
    except OSError:
        return "unknown"
    return output.splitlines()[0] if output else "unknown"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--spp", type=Path, default=ROOT / "cmake-build-release-wsl-dev" / "spp")
    parser.add_argument("--out", type=Path, default=Path("benchmark-record.json"))
    parser.add_argument("--workloads", nargs="+", default=None, help="default: every workload under compile/")
    parser.add_argument("--modes", nargs="+", default=["dev", "rel"], choices=["dev", "rel"])
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--cachegrind", action="store_true", help="also count instructions, once per workload and mode")
    parser.add_argument("--std-git", default=STD_GIT, help="where to mirror the std from (a local clone works)")
    parser.add_argument("--work", type=Path, default=None, help="keep the projects here instead of a temporary folder")
    args = parser.parse_args()

    spp = args.spp.resolve()
    if not spp.exists():
        sys.exit(f"error: no spp at {spp}")
    rev = (HERE / "std.rev").read_text().strip()
    names = args.workloads or workload_names()
    unknown = sorted(set(names) - set(workload_names()))
    if unknown:
        sys.exit(f"error: no workload {', '.join(unknown)}; there are {', '.join(workload_names())}")

    work = args.work.resolve() if args.work else Path(tempfile.mkdtemp(prefix="spp-bench-"))
    work.mkdir(parents=True, exist_ok=True)
    try:
        mirror = std_mirror(work, args.std_git, rev)
        workloads: dict[str, dict] = {}
        spp_version = "unknown"
        for name in names:
            project = work / "projects" / name
            digest = make_project(name, project, mirror, rev)
            if run([str(spp), "vcs"], cwd=project).returncode != 0:
                sys.exit(f"error: could not check out the std at {rev} for {name}")
            modes = {}
            for mode in args.modes:
                log(f"{name} ({mode}): {args.rounds} round(s){' and cachegrind' if args.cachegrind else ''}")
                modes[mode] = measure(spp, project, mode, args.rounds, args.cachegrind)
                spp_version = modes[mode]["spp"]
                if not modes[mode]["deterministic"]:
                    log(f"warning: {name} ({mode}) gave different counters in different rounds")
            workloads[name] = {"hash": digest, "modes": modes}

        record = {
            "schema": 1,
            "keys": {
                "spp": spp_version,
                "spp_commit": spp_commit(),
                "llvm": llvm_version(spp),
                "std_rev": rev,
                "cpu": cpu_model(),
                "os": platform.platform(),
                # GitHub's own names, and environment names are case-sensitive.
                "runner_image": f"{os.environ.get('ImageOS', '')} {os.environ.get('ImageVersion', '')}".strip(),  # noqa: SIM112
                "valgrind": tool_version(["valgrind", "--version"]) if args.cachegrind else None,
                "rounds": args.rounds,
            },
            "workloads": workloads,
        }
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
        log(f"Wrote {args.out}")
    finally:
        if not args.work:
            shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
