#!/usr/bin/env bash

# End-to-end check of "spp build --print-executable", which a tool (spp-idea's
# debugger) asks for the executable it then launches itself. Asserts:
#   - it prints one line, an absolute path, and builds nothing;
#   - a build then links exactly there, at dev and rel, each to its own path;
#   - on a "build = lib" project it fails, saying it is a library, and prints
#     no path.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-print-executable.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

project="${WORK}/printexe"
new_project "${project}"
cat > "${project}/src/main.spp" <<'SPP'
fun main() -> Void {
  std::console::println("ran")
}
SPP

declare -A printed
for mode in dev rel; do
    name="${mode}: the printed path"
    out="$(cd "${project}" && "${SPP}" build -m "${mode}" --print-executable 2> "${project}/print.log")"
    rc=$?
    if (( rc != 0 )); then
        fail "${name}: exit ${rc}" "${project}/print.log"
        continue
    elif [[ "$(wc -l <<< "${out}")" -ne 1 || "${out}" != /* ]]; then
        fail "${name}: not one absolute path: ${out}"
        continue
    elif [[ -d "${project}/out/$(basename "$(dirname "$(dirname "${out}")")")/${mode}/llvm" ]]; then
        fail "${name}: it built something"
        continue
    fi
    printed[${mode}]="${out}"

    (cd "${project}" && "${SPP}" build -m "${mode}" > "${project}/build.log" 2>&1)
    if [[ -x "${out}" ]] && [[ "$("${out}")" == "ran" ]]; then
        ok "${name}: built to and runs from ${out#"${project}/"}"
    else
        fail "${name}: nothing runnable at ${out}" "${project}/build.log"
    fi
done
if [[ -n "${printed[dev]:-}" && "${printed[dev]}" == "${printed[rel]:-}" ]]; then
    fail "dev and rel print the same path"
fi

sed -i 's/^build = "exe"$/build = "lib"/' "${project}/spp.toml"
out="$(cd "${project}" && "${SPP}" build --print-executable 2> "${project}/print.log")"
rc=$?
if (( rc == 0 )) || [[ -n "${out}" ]]; then
    fail "a lib: exit ${rc}, printed \"${out}\"" "${project}/print.log"
elif ! grep -q "is a library" "${project}/print.log"; then
    fail "a lib: failed without saying it is a library" "${project}/print.log"
else
    ok "a lib: refused, no path"
fi

finish
