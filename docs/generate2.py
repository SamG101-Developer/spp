import os
import shutil
import subprocess
from pathlib import Path


class DocumentGenerator:
    root: Path
    docs_dir: Path
    source_dir: Path
    build_dir: Path
    api_dir: Path
    src_dir: Path
    doxygen_xml_dir: Path
    doxygen_config: Path
    index_file: Path
    module_map: dict[str, list[str]]

    def __init__(self):
        self.root = Path(__file__).parent.parent.resolve()
        self.docs_dir = self.root / "docs"
        self.source_dir = self.docs_dir / "source"
        self.build_dir = self.docs_dir / "build"
        self.api_dir = self.source_dir / "compiler-api"
        self.src_dir = self.root / "headers" / "spp"
        self.doxygen_xml_dir = self.build_dir / "doxygen" / "xml"
        self.doxygen_config = self.root / "Doxyfile"
        self.index_file = self.build_dir / "index.rst"
        self.module_map = {}

    def _format_module_or_package(self, path: Path) -> str:
        return path.with_suffix("").as_posix().replace("/", ".")

    def _validate_package(self, module: Path) -> None:
        package = self._format_module_or_package(module.parent)
        if package not in self.module_map:
            if module.parent.parent != Path("."):
                self._validate_package(module.parent)
            self.module_map[package] = [self._format_module_or_package(module)]
            return
        self.module_map[package].append(self._format_module_or_package(module))

    def generate(self):
        # Clear old build files.
        shutil.rmtree(self.api_dir, ignore_errors=True)
        shutil.rmtree(self.build_dir, ignore_errors=True)

        # Generate the new module files for "compiler-api".
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.api_dir.mkdir(parents=True, exist_ok=True)
        self.doxygen_xml_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run(["doxygen", str(self.doxygen_config)], check=True)

        # Generate the index file.
        for header in self.src_dir.rglob("*.hpp"):
            self._validate_package(header.relative_to(self.src_dir.parent))

        # Iterate the module map and generate "package" files.
        for package, modules in self.module_map.items():
            with open(self.api_dir / (package + ".rst"), "w") as package_file:
                package_file.write(f"{package}\n")
                package_file.write("=" * len(package) + "\n\n")
                package_file.write(".. toctree::\n")
                package_file.write("   :maxdepth: 1\n\n")
                for module in sorted(modules):
                    package_file.write(f"   {module}\n")

            for module in modules:
                with open(self.api_dir / (module + ".rst"), "w") as module_file:
                    module_file.write(f"{module}\n")
                    module_file.write("-" * len(module) + "\n\n")
                    module_file.write(f".. doxygenfile:: {module.replace('.', '/')}.hpp\n")
                    module_file.write("   :project: s++\n")
                    # module_file.write("   :sections: define enum func typedef var\n")

        # Compiler docs root index file.
        with open(self.api_dir / "index.rst", "w") as f:
            f.write("s++ Compiler API\n")
            f.write("================\n\n")
            f.write(".. toctree::\n")
            f.write("   :maxdepth: 1\n\n")
            f.write(f"   spp\n")

        # Root index file (special handling).
        with open(self.index_file, "w") as f:
            f.write("s++ Compiler API\n")
            f.write("================\n\n")
            f.write(".. toctree::\n")
            f.write("   :maxdepth: 1\n\n")
            f.write("   :caption: S++ Documentation\n\n")
            f.write("   compiler-api\n")
            f.write("   spec-api\n")

        # Final build step.
        os.chdir(self.docs_dir)
        subprocess.run(["make", "html"], check=True)


if __name__ == "__main__":
    generator = DocumentGenerator()
    generator.generate()
