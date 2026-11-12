#!/usr/bin/env bash

# End-to-end check of where a tool learns the executable it launches itself
# (spp-idea's debugger): "spp info executable", and the artifact line a json
# build ends with. Asserts:
#   - "spp info executable" prints one line, an absolute path, and builds
#     nothing;
#   - a build then links exactly there, at dev and rel, each to its own path;
#   - "spp build --message-format=json" ends with
#     {"kind":"artifact","executable":"<that path>"}, and an analysis-only one
#     with no artifact;
#   - on a "build = lib" project "spp info executable" fails, saying it is a
#     library, and prints no path.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-info-executable.sh [path/to/spp]

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
    out="$(cd "${project}" && "${SPP}" info executable -m "${mode}" 2> "${project}/print.log")"
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

    # The same path, as the last line of a json build.
    name="${mode}: the json build's artifact"
    (cd "${project}" && "${SPP}" build -m "${mode}" --skip-vcs --message-format=json > "${project}/json.log" 2>&1)
    expected="{\"kind\":\"artifact\",\"executable\":\"${out}\"}"
    if [[ "$(tail -1 "${project}/json.log")" == "${expected}" ]]; then
        ok "${name}: names ${out#"${project}/"}"
    else
        fail "${name}: the last line is not ${expected}" "${project}/json.log"
    fi
done

(cd "${project}" && "${SPP}" build --skip-vcs --analyse-only --message-format=json > "${project}/analyse.log" 2>&1)
if grep -qF '"kind":"artifact"' "${project}/analyse.log"; then
    fail "an analysis-only json build: names an artifact" "${project}/analyse.log"
else
    ok "an analysis-only json build: no artifact"
fi
if [[ -n "${printed[dev]:-}" && "${printed[dev]}" == "${printed[rel]:-}" ]]; then
    fail "dev and rel print the same path"
fi

sed -i 's/^build = "exe"$/build = "lib"/' "${project}/spp.toml"
out="$(cd "${project}" && "${SPP}" info executable 2> "${project}/print.log")"
rc=$?
if (( rc == 0 )) || [[ -n "${out}" ]]; then
    fail "a lib: exit ${rc}, printed \"${out}\"" "${project}/print.log"
elif ! grep -q "is a library" "${project}/print.log"; then
    fail "a lib: failed without saying it is a library" "${project}/print.log"
else
    ok "a lib: refused, no path"
fi

finish
