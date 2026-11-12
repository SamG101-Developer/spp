#!/usr/bin/env bash

# End-to-end check that a program's arguments reach "std::process::args": "spp
# run" passes what follows "--" on, the compiler's C entry point hands argv to
# sppc ("sppc_set_args"), and std reads them back. A program that prints each
# of its arguments, one per line, the program itself first. Asserts:
#   - "spp run -- a 'b c' --mode '$HOME'" gives it exactly those four
#     arguments after its own name, unsplit and unexpanded;
#   - "spp run" gives it its own name alone;
#   - an argument without "--" that spp does not know is still refused.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-run-args.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

project="${WORK}/runargs"
new_project "${project}"
cat > "${project}/src/main.spp" <<'SPP'
fun main() -> Void {
  let args = std::process::args()
  loop arg in args.iter_ref() {
    std::console::println(arg)
  }
  std::mem::ops::drop(args)
}
SPP

# What the program printed: everything after "spp run" says it is running it,
# without the escapes it clears the console with first.
printed() {
    sed -n '/^Running: /,$p' "${project}/run.log" | tail -n +2 | sed 's/\x1b\[[0-9;]*[A-Za-z]//g'
}

# shellcheck disable=SC2016 # a literal "$HOME": the program must receive it unexpanded
(cd "${project}" && "${SPP}" run -- a 'b c' --mode '$HOME' > "${project}/run.log" 2>&1)
rc=$?
got="$(printed)"
expected=$'a\nb c\n--mode\n$HOME'
if (( rc != 0 )); then
    fail "spp run -- ...: exit ${rc}" "${project}/run.log"
elif [[ "$(head -1 <<< "${got}")" != */runargs ]]; then
    fail "spp run -- ...: the program's own name is not first: ${got//$'\n'/ | }" "${project}/run.log"
elif [[ "$(tail -n +2 <<< "${got}")" != "${expected}" ]]; then
    fail "spp run -- ...: got: ${got//$'\n'/ | }" "${project}/run.log"
else
    ok "spp run -- a 'b c' --mode '\$HOME': its name, then four arguments, whole"
fi

(cd "${project}" && "${SPP}" run > "${project}/run.log" 2>&1)
rc=$?
got="$(printed)"
if (( rc == 0 )) && [[ "$(wc -l <<< "${got}")" -eq 1 && "${got}" == */runargs ]]; then
    ok "spp run: its own name alone"
else
    fail "spp run: exit ${rc}, got: ${got//$'\n'/ | }" "${project}/run.log"
fi

if (cd "${project}" && "${SPP}" run --frobnicate > "${project}/run.log" 2>&1); then
    fail "spp run --frobnicate: accepted" "${project}/run.log"
else
    ok "spp run --frobnicate: refused"
fi

finish
