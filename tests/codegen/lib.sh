# shellcheck shell=bash
# What every codegen check shares: a scratch project with std in it, an ffi
# library built into it, a build, a run, and the tally. Sourced with the
# check's own arguments:
#
#   source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"
#
# The first argument is the spp binary (the release build by default), and
# SPPC_LIB, if set, replaces the vendored libsppc.so in every project, to check
# a runtime that has not been shipped into SPP-STL yet.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly ROOT
SPP="$(realpath "${1:-${ROOT}/cmake-build-release-wsl-dev/spp}")"
readonly SPP
readonly FIXTURE="${ROOT}/tests/test_outputs"
readonly SPPC_LIB="${SPPC_LIB:-}"
HOST_FFI="$(uname -m)-linux-gnu"
readonly HOST_FFI
readonly STD_GIT="https://github.com/SamG101-Developer/SPP-STL"

WORK="$(mktemp -d)"
readonly WORK
trap 'rm -rf "${WORK}"' EXIT

failures=0

# Where projects copy std from: the gtest fixture's checkout when there is one,
# or else one clone per script run, rather than one per build. Each build
# still pulls it, so either is current.
std_vcs() {
    if [[ -d "${FIXTURE}/vcs/std" ]]; then
        echo "${FIXTURE}/vcs"
        return
    fi
    if [[ ! -d "${WORK}/vcs/std" ]]; then
        git clone -q --branch master "${STD_GIT}" "${WORK}/vcs/std" >&2
    fi
    echo "${WORK}/vcs"
}

# A project in "dir", named after it (the executable takes the project
# directory's name), with std and an "spp.toml" holding "config" after the
# [project] and [vcs] sections.
new_project() {
    local dir="$1" config="${2:-}"
    mkdir -p "${dir}/src"
    cp -r "$(std_vcs)" "${dir}/vcs"
    # Pulled first, so the build's own pull has nothing to merge over the
    # replaced library.
    if [[ -n "${SPPC_LIB}" ]]; then
        git -C "${dir}/vcs/std" pull -q origin master
        find "${dir}" -name libsppc.so -exec cp "${SPPC_LIB}" {} \;
    fi

    cat > "${dir}/spp.toml" <<EOF
[project]
name = "$(basename "${dir}")"
version = "0.1.0"
build = "exe"

[vcs]
std = { git = "${STD_GIT}", branch = "master" }
${config}
EOF
}

# The ffi package "pkg": a host library built from "source" (C, or C++ for a
# ".cpp"), hardened the way the runtime is, and the stub read from stdin.
add_ffi() {
    local dir="$1" pkg="$2" source="$3"
    local cc=cc
    [[ "${source}" == *.cpp ]] && cc=c++
    mkdir -p "${dir}/ffi/${pkg}/${HOST_FFI}"
    "${cc}" -shared -fPIC -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -Wl,-z,relro,-z,now \
        -o "${dir}/ffi/${pkg}/${HOST_FFI}/lib${pkg}.so" "${source}"
    cat > "${dir}/ffi/${pkg}/stub.spp"
}

# Build "dir" at "mode", leaving the executable in EXE and the program's own
# object in OBJ. On a failure, reports it against "name" and returns 1; the
# caller counts it.
build_project() {
    local name="$1" dir="$2" mode="$3"
    EXE="" OBJ=""
    if ! (cd "${dir}" && "${SPP}" build -m "${mode}" > "${dir}/build.log" 2>&1); then
        echo "FAIL  ${name}: build failed, see below"
        tr '\r' '\n' < "${dir}/build.log" | grep -v '^\s*$' | tail -20
        return 1
    fi
    EXE="$(find "${dir}/out" -type f -path "*/${mode}/$(basename "${dir}")" | head -1)"
    OBJ="$(find "${dir}/out" -type f -path "*/${mode}/llvm/spp.o" | head -1)"
    if [[ -z "${EXE}" || -z "${OBJ}" ]]; then
        echo "FAIL  ${name}: no executable or object under ${dir}/out"
        return 1
    fi
}

# Run "exe" with its output in "log" and its exit code in RC. Never through a
# pipe, which would report the pipe's status and hide a crash. A run still
# going after RUN_TIMEOUT seconds (60) is killed and failed here: no check
# expects a hang, and one that hangs the same way twice compares equal.
run_exe() {
    timeout --kill-after=5 "${RUN_TIMEOUT:-60}" "$1" > "$2" 2>&1
    # shellcheck disable=SC2034 # read by the checks
    RC=$?
    if (( RC == 124 || RC == 137 )); then
        echo "timed out after ${RUN_TIMEOUT:-60}s" >> "$2"
        fail "$(basename "$1"): still running after ${RUN_TIMEOUT:-60}s" "$2"
    fi
}

ok() {
    echo "ok    $1"
}

# Report a failed case, with the head of "log" (if given) under it.
fail() {
    local message="$1" log="${2:-}"
    echo "FAIL  ${message}"
    [[ -n "${log}" ]] && sed 's/^/        /' "${log}" | head -12
    failures=$((failures + 1))
}

# For a check that reads one architecture's disassembly.
require_x86_64() {
    if [[ "$(uname -m)" != "x86_64" ]]; then
        echo "skipped: the disassembly checked is x86's"
        exit 0
    fi
}

finish() {
    if (( failures > 0 )); then
        echo "${failures} case(s) failed"
        exit 1
    fi
    echo "all cases passed"
}
