import argparse
import shutil
import subprocess
import sys
from pathlib import Path

# Header extensions that Doxygen is pointed at: the .hpp and .ixx files.
HEADER_SUFFIXES = (".ixx", ".hpp")

# The Sphinx builders this script exposes. The requested builder is looked up here rather than used directly, so the
# string that reaches the command line is always one of these literals and never the raw CLI argument.
SPHINX_BUILDERS = {
    "html": "html",
    "latex": "latex",
    "man": "man",
    "text": "text",
    "xml": "xml",
}


class DocumentGenerator:
    root: Path
    docs_dir: Path
    source_dir: Path
    build_dir: Path
    api_dir: Path
    src_dir: Path
    doxygen_xml_dir: Path
    doxygen_config: Path
    module_map: dict[str, set[str]]

    def __init__(self) -> None:
        self.root = Path(__file__).parent.parent.resolve()
        self.docs_dir = self.root / "docs"
        self.source_dir = self.docs_dir / "source"
        self.build_dir = self.docs_dir / "build"
        self.api_dir = self.source_dir / "compiler-api"
        self.src_dir = self.root / "headers" / "spp"
        self.doxygen_xml_dir = self.build_dir / "doxygen" / "xml"
        self.doxygen_config = self.root / "Doxyfile"
        self.module_map = {}

    def _format_module_or_package(self, path: Path) -> str:
        return path.with_suffix("").as_posix().replace("/", ".")

    def _register(self, module: Path) -> None:
        """Register a header under its package, and that package under each of its ancestors, up to the root."""
        child = self._format_module_or_package(module)
        directory = module.parent
        while True:
            package = self._format_module_or_package(directory)
            self.module_map.setdefault(package, set()).add(child)
            if directory.parent == Path("."):
                return
            child, directory = package, directory.parent

    def _write_toctree(self, path: Path, title: str, underline: str, entries: list[str], maxdepth: int = 1) -> None:
        with open(path, "w", encoding="utf-8") as f:
            f.write(f"{title}\n")
            f.write(underline * len(title) + "\n\n")
            f.write(".. toctree::\n")
            f.write(f"   :maxdepth: {maxdepth}\n\n")
            for entry in entries:
                f.write(f"   {entry}\n")

    def run_doxygen(self) -> None:
        self.doxygen_xml_dir.mkdir(parents=True, exist_ok=True)

        # The Doxyfile's INPUT, XML_OUTPUT and INPUT_FILTER paths are all relative to docs/, so doxygen has to be run
        # from there regardless of where this script was invoked from.
        subprocess.run(["doxygen", str(self.doxygen_config)], check=True, cwd=self.docs_dir)

        if not any(self.doxygen_xml_dir.glob("*.xml")):
            raise RuntimeError(f"doxygen produced no XML in {self.doxygen_xml_dir}")

    def generate_compiler_api(self) -> None:
        headers = sorted(h for h in self.src_dir.rglob("*") if h.suffix in HEADER_SUFFIXES)
        if not headers:
            raise RuntimeError(f"no {'/'.join(HEADER_SUFFIXES)} headers found under {self.src_dir}")

        # Map every header onto a dotted module name, and register it up the package chain.
        suffix_of: dict[str, str] = {}
        for header in headers:
            relative = header.relative_to(self.src_dir.parent)
            self._register(relative)
            suffix_of[self._format_module_or_package(relative)] = header.suffix

        # A package page lists its own modules and its sub-packages, so the toctree is navigable from the root down.
        for package, children in self.module_map.items():
            self._write_toctree(self.api_dir / f"{package}.rst", package, "=", sorted(children))

        for module, suffix in suffix_of.items():
            # breathe resolves doxygenfile against the path doxygen recorded, which STRIP_FROM_PATH reduces to a path
            # relative to headers/ (eg "spp/asts/ast.ixx").
            file_path = module.replace(".", "/") + suffix
            with open(self.api_dir / f"{module}.rst", "w", encoding="utf-8") as f:
                f.write(f"{module}\n")
                f.write("-" * len(module) + "\n\n")
                f.write(f".. doxygenfile:: {file_path}\n")
                f.write("   :project: s++\n")

        self._write_toctree(self.api_dir / "index.rst", "s++ Compiler API", "=", ["spp"], maxdepth=2)

    def run_sphinx(self, builder: str, strict: bool) -> Path:
        if builder not in SPHINX_BUILDERS:
            raise RuntimeError(f"unknown builder {builder!r} (expected one of {', '.join(sorted(SPHINX_BUILDERS))})")
        name = SPHINX_BUILDERS[builder]

        out_dir = self.build_dir / name
        command = [sys.executable, "-m", "sphinx", "-b", name, str(self.source_dir), str(out_dir)]
        if strict:
            # -W promotes warnings to errors; --keep-going reports all of them rather than dying on the first.
            command += ["-W", "--keep-going"]
        subprocess.run(command, check=True, cwd=self.docs_dir)
        return out_dir

    def generate(self, skip_doxygen: bool = False, strict: bool = False, builder: str = "html") -> None:
        # Wipe the generated tree so pages for deleted headers do not linger. The spec tree is entirely hand-written and
        # is never touched.
        shutil.rmtree(self.api_dir, ignore_errors=True)

        self.api_dir.mkdir(parents=True, exist_ok=True)
        self.build_dir.mkdir(parents=True, exist_ok=True)

        if skip_doxygen:
            if not any(self.doxygen_xml_dir.glob("*.xml")):
                raise RuntimeError(f"--skip-doxygen given but no existing XML in {self.doxygen_xml_dir}")
        else:
            self.run_doxygen()

        self.generate_compiler_api()
        out_dir = self.run_sphinx(builder, strict)

        print(f"documentation written to {out_dir}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--skip-doxygen", action="store_true", help="reuse the existing doxygen XML")
    parser.add_argument("--strict", action="store_true", help="treat Sphinx warnings as errors")
    parser.add_argument("--builder", default="html", choices=sorted(SPHINX_BUILDERS), help="Sphinx builder to run (default: html)")
    args = parser.parse_args()

    try:
        DocumentGenerator().generate(skip_doxygen=args.skip_doxygen, strict=args.strict, builder=args.builder)
    except (subprocess.CalledProcessError, RuntimeError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
