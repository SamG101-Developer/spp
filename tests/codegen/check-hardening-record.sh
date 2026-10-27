#!/usr/bin/env bash

# End-to-end check that every executable records what it was built with, and
# that "spp audit" reads it back. Builds a scratch project with two keys turned
# off, and asserts, at dev and rel:
#   - the record names the compiler version, the target and the mode;
#   - it holds every on/off key "spp config" lists, the two set ones false, a
#     defaulted one true, and the run-time measures the default floor cannot
#     have as left out, with the floor;
#   - "strip" leaves it in place (an ELF note);
# and that a program spp did not build is reported as having no record.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-hardening-record.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

VERSION="$(head -1 "${ROOT}/VERSION" | tr -d '[:space:]')"
readonly VERSION
KEYS="$("${SPP}" config | grep -cE '^    - [a-z_]+ = (true|false)  \(default\)$')"
readonly KEYS

check() {
    local mode="$1"
    local name="${mode}"
    local dir="${WORK}/${mode}/recorded"
    new_project "${dir}" "
[binary.codegen]
cfi = false
sls = false"
    cat > "${dir}/src/main.spp" <<'EOF'
fun main() -> Void {
  std::console::println("hello")
}
EOF
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }

    if ! "${SPP}" audit "${EXE}" > "${dir}/audit.log" 2>&1; then
        fail "${name}: spp audit failed" "${dir}/audit.log"
        return
    fi

    local missing=""
    for line in "spp ${VERSION}" "target ${HOST_FFI}" "mode ${mode}" \
        "binary.codegen.cfi = false" "binary.codegen.sls = false" "memory.stack.protect = true" \
        "memory.map.deny_wx = left out (needs Linux 6.6, above the floor 4.18)" \
        "memory.map.seal = left out (needs Linux 6.10, above the floor 4.18)" "target.floor.linux = 4.18"; do
        grep -qxF "${line}" "${dir}/audit.log" || missing="${line}"
    done
    local recorded
    recorded="$(grep -cE '^[a-z_.]+ = (true|false|left out \(.*\))$' "${dir}/audit.log")"

    cp "${EXE}" "${dir}/stripped"
    strip "${dir}/stripped"
    local stripped_ok=1
    "${SPP}" audit "${dir}/stripped" 2>&1 | cmp -s - "${dir}/audit.log" || stripped_ok=0

    if [[ -n "${missing}" ]]; then
        fail "${name}: the record has no \"${missing}\"" "${dir}/audit.log"
    elif (( recorded != KEYS )); then
        fail "${name}: the record holds ${recorded} keys, \"spp config\" lists ${KEYS}" "${dir}/audit.log"
    elif (( stripped_ok == 0 )); then
        fail "${name}: the stripped executable's record differs"
    else
        ok "${name}: ${recorded} keys recorded, and kept by strip"
    fi
}

for mode in dev rel; do
    check "${mode}"
done

# A program spp did not build has no record, and says so.
if "${SPP}" audit /bin/true > "${WORK}/foreign.log" 2>&1; then
    fail "a program spp did not build: audit succeeded" "${WORK}/foreign.log"
elif grep -q "has no hardening record" "${WORK}/foreign.log"; then
    ok "a program spp did not build: no record"
else
    fail "a program spp did not build: not reported as having no record" "${WORK}/foreign.log"
fi

finish
