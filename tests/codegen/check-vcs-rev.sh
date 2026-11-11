#!/usr/bin/env bash

# End-to-end check of a [vcs] record pinned with "rev". std itself is the
# dependency pinned, from a local clone standing in for its remote. Asserts:
#   - "spp vcs" clones a pinned dependency and checks out the pinned commit,
#     detached, even when it is not the remote's newest;
#   - fetching again at the same pin needs no remote at all;
#   - moving the pin moves the checkout;
#   - a checkout away from its pin is refused by "spp validate", naming the pin;
#   - a pin naming a commit the remote does not have fails the fetch;
#   - a project builds and runs against the pinned std.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-vcs-rev.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

readonly REMOTE="${WORK}/remotes/std"
git clone -q "$(std_vcs)/std" "${REMOTE}"
NEWEST="$(git -C "${REMOTE}" rev-parse HEAD)"
OLDER="$(git -C "${REMOTE}" rev-parse HEAD~1)"
readonly NEWEST OLDER

project="${WORK}/pinned"
mkdir -p "${project}/src"
cat > "${project}/src/main.spp" <<'EOF'
fun main() -> Void { }
EOF

# The project's "spp.toml", with std pinned to "rev".
pin_std() {
    cat > "${project}/spp.toml" <<EOF
[project]
name = "pinned"
version = "0.1.0"
build = "exe"

[vcs]
std = { git = "${REMOTE}", rev = "$1" }
EOF
}

head_of_std() {
    cat "${project}/vcs/std/.git/HEAD" 2>/dev/null
}

# "spp vcs" in the project, with its output in "log"; true if it succeeded.
fetch() {
    (cd "${project}" && "${SPP}" vcs > "$1" 2>&1)
}

pin_std "${OLDER}"
if fetch "${project}/clone.log" && [[ "$(head_of_std)" == "${OLDER}" ]]; then
    ok "a pin older than the remote's newest: cloned and checked out, detached"
else
    fail "clone: expected HEAD ${OLDER}, got '$(head_of_std)'" "${project}/clone.log"
fi

mv "${REMOTE}" "${REMOTE}.away"
if fetch "${project}/again.log" && [[ "$(head_of_std)" == "${OLDER}" ]]; then
    ok "the same pin again: no remote needed"
else
    fail "the same pin again: failed without the remote" "${project}/again.log"
fi
mv "${REMOTE}.away" "${REMOTE}"

pin_std "${NEWEST}"
if fetch "${project}/move.log" && [[ "$(head_of_std)" == "${NEWEST}" ]]; then
    ok "a moved pin: the checkout moves with it"
else
    fail "a moved pin: expected HEAD ${NEWEST}, got '$(head_of_std)'" "${project}/move.log"
fi

pin_std "${OLDER}"
if (cd "${project}" && "${SPP}" validate > "${project}/validate.log" 2>&1); then
    fail "a checkout away from its pin: validated" "${project}/validate.log"
elif tr '\r' '\n' < "${project}/validate.log" | grep -qF "is pinned to ${OLDER}"; then
    ok "a checkout away from its pin: refused, naming the pin"
else
    fail "a checkout away from its pin: refused without naming the pin" "${project}/validate.log"
fi

pin_std "0000000000000000000000000000000000000000"
if fetch "${project}/unknown.log"; then
    fail "a pin the remote does not have: the fetch succeeded" "${project}/unknown.log"
else
    ok "a pin the remote does not have: the fetch fails"
fi

pin_std "${NEWEST}"
if [[ -n "${SPPC_LIB}" ]]; then
    fetch "${project}/sppc.log"
    find "${project}/vcs" -name libsppc.so -exec cp "${SPPC_LIB}" {} \;
fi
if build_project "against the pinned std" "${project}" dev; then
    run_exe "${EXE}" "${project}/run.log"
    if [[ ${RC} -eq 0 && "$(head_of_std)" == "${NEWEST}" ]]; then
        ok "a project builds and runs against the pinned std"
    else
        fail "against the pinned std: exit ${RC}, HEAD '$(head_of_std)'" "${project}/run.log"
    fi
else
    failures=$((failures + 1))
fi

finish
