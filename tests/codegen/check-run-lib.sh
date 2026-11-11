#!/usr/bin/env bash

# End-to-end check that "spp run" refuses a library before building it. Asserts:
#   - "spp run" on a "build = lib" project fails, says it is a library, and
#     compiles nothing;
#   - "spp build" on the same library still succeeds;
#   - control: the same project as "build = exe" runs.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-run-lib.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

project="${WORK}/runlib"
new_project "${project}"
cat > "${project}/src/main.spp" <<'EOF'
fun main() -> Void {
  std::console::println("ran")
}
EOF

sed -i 's/^build = "exe"$/build = "lib"/' "${project}/spp.toml"
(cd "${project}" && "${SPP}" run > "${project}/run.log" 2>&1)
rc=$?
if (( rc == 0 )); then
    fail "spp run on a lib: succeeded" "${project}/run.log"
elif ! grep -q "is a library" "${project}/run.log"; then
    fail "spp run on a lib: failed without saying it is a library" "${project}/run.log"
elif [[ -n "$(find "${project}/out" -type d -name llvm 2>/dev/null)" ]]; then
    fail "spp run on a lib: compiled before refusing" "${project}/run.log"
else
    ok "spp run on a lib: refused before building"
fi

if (cd "${project}" && "${SPP}" build > "${project}/build.log" 2>&1); then
    ok "spp build on a lib: builds"
else
    fail "spp build on a lib: failed" "${project}/build.log"
fi

sed -i 's/^build = "lib"$/build = "exe"/' "${project}/spp.toml"
(cd "${project}" && "${SPP}" run > "${project}/run.log" 2>&1)
rc=$?
if (( rc == 0 )) && grep -q "ran$" "${project}/run.log"; then
    ok "spp run on an exe: runs (control)"
else
    fail "spp run on an exe: rc ${rc}" "${project}/run.log"
fi

finish
