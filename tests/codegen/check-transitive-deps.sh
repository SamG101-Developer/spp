#!/usr/bin/env bash

# End-to-end check that a dependency's own dependencies are fetched, compiled and
# linked. The project declares "a", and "a" declares "b", whose ffi library is
# only ever called by "b". Real git repositories on disk stand in for the remote
# ones. Asserts:
#   - the project builds, finding "b" flat under "vcs", and runs through to "b"'s
#     library, including when "b" declares "a" back;
#   - "spp test -l b" runs the tests of a dependency the project never declared;
#   - "a" and the project declaring "b" on two branches fails, naming both;
#   - a removed dependency, and what only it declared, stop being compiled;
#   - the project holding a value of "b"'s type, through "a", is fine, but writing
#     "b::..." is E121, as it is for "a" if it has not declared "b";
#   - a project or dependency not declaring std, and two dependencies sharing a
#     top-level namespace, fail before anything is compiled, naming what is wrong.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-transitive-deps.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/greet.c" <<'EOF2'
#include <stdio.h>

void probe_greet(void) {
  puts("hello");
  fflush(stdout);
}
EOF2

readonly REMOTES="${WORK}/remotes"

# Commit everything in the repository at "dir", creating it the first time.
commit_all() {
    local dir="$1"
    [[ -d "${dir}/.git" ]] || git -C "${dir}" init -q -b master
    git -C "${dir}" add -A
    git -C "${dir}" -c user.name=check -c user.email=check@example.com commit -q -m "state"
}

# The "spp.toml" of a library declaring std and "vcs" (more records).
library_toml() {
    local dir="$1" vcs="${2:-}"
    cat > "${dir}/spp.toml" <<EOF2
[project]
name = "$(basename "${dir}")"
version = "0.1.0"
build = "lib"

[vcs]
std = { git = "${STD_GIT}", branch = "master" }
${vcs}
EOF2
}

# Library "b": one public function calling its own ffi package, and a test.
make_b() {
    local dir="${REMOTES}/b"
    mkdir -p "${dir}/src/b" "${dir}/tst/b"
    library_toml "${dir}" "${1:-}"
    add_ffi "${dir}" probe "${WORK}/greet.c" <<'EOF2'
use std::cffi::CVoid

!ffi(symbol="probe_greet")
fun probe_greet() -> CVoid { }
EOF2
    cat > "${dir}/src/b/api.spp" <<'EOF2'
!public
fun greet() -> Void {
  probe::probe_greet()
}

!public
cls Thing { }

sup Thing ext Copy { }

sup Thing {
  !public
  fun speak(&self) -> Void {
    probe::probe_greet()
  }
}
EOF2
    cat > "${dir}/tst/b/api.spp" <<'EOF2'
!test(group="b/api")
fun test_from_b() -> Void {
  b::api::greet()
}
EOF2
    commit_all "${dir}"
}

# Library "a": public functions calling "b", and handing out one of its values.
make_a() {
    local dir="${REMOTES}/a"
    mkdir -p "${dir}/src/a"
    library_toml "${dir}" "${1-b = { git = \"${REMOTES}/b\", branch = \"master\" \}}"
    cat > "${dir}/src/a/api.spp" <<'EOF2'
!public
fun run() -> Void {
  b::api::greet()
}

!public
fun make() -> b::api::Thing {
  ret b::api::Thing()
}
EOF2
    commit_all "${dir}"
}

# A project declaring "a" and "config", with a main going through it.
make_project() {
    local dir="$1" config="${2:-}"
    new_project "${dir}" "a = { git = \"${REMOTES}/a\", branch = \"master\" }
${config}"
    cat > "${dir}/src/main.spp" <<'EOF2'
fun main() -> Void {
  a::api::run()
}
EOF2
}

check_runs() {
    local name="$1" dir="$2"
    build_project "${name}" "${dir}" dev || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -eq 0 ]] && grep -qx "hello" "${dir}/run.log" && [[ -f "${dir}/vcs/b/spp.toml" ]]; then
        ok "${name}: builds, and runs through to b's library"
    else
        fail "${name}: expected \"hello\", exit 0 and vcs/b (exit ${RC})" "${dir}/run.log"
    fi
}

make_b
make_a

project="${WORK}/plain/app"
make_project "${project}"
check_runs "a dependency of a dependency" "${project}"

# The tests of "b", which the project never declared.
if (cd "${project}" && "${SPP}" test -l b --filter test_from_b > "${project}/test.log" 2>&1) &&
    tr '\r' '\n' < "${project}/test.log" | grep -qF "test_from_b"; then
    ok "spp test -l b: runs a transitive dependency's tests"
else
    fail "spp test -l b: did not run test_from_b" "${project}/test.log"
fi

# A removed dependency goes, with what only it declared. Broken source in either
# would fail the build if it was still read.
printf 'this is not s++\n' > "${project}/vcs/a/src/a/broken.spp"
printf 'this is not s++\n' > "${project}/vcs/b/src/b/broken.spp"
sed -i '/^a = /d' "${project}/spp.toml"
cat > "${project}/src/main.spp" <<'EOF2'
fun main() -> Void { }
EOF2
if (cd "${project}" && "${SPP}" build -m dev > "${project}/removed.log" 2>&1); then
    ok "a removed dependency: it and what only it declared stop being compiled"
else
    fail "a removed dependency: still read" "${project}/removed.log"
fi

# The same name asked for on two branches, from two packages.
project="${WORK}/clash/app"
make_project "${project}" "b = { git = \"${REMOTES}/b\", branch = \"other\" }"
if (cd "${project}" && "${SPP}" vcs > "${project}/clash.log" 2>&1); then
    fail "two branches: the fetch succeeded" "${project}/clash.log"
elif tr '\r' '\n' < "${project}/clash.log" | grep -qF "this project declares" &&
    tr '\r' '\n' < "${project}/clash.log" | grep -qF "the dependency 'a' declares"; then
    ok "two branches: refused, naming both"
else
    fail "two branches: refused without naming both" "${project}/clash.log"
fi

# A value of "b"'s type, reached through "a": held and used without naming "b".
project="${WORK}/held/app"
make_project "${project}"
cat > "${project}/src/main.spp" <<'EOF2'
fun main() -> Void {
  let thing = a::api::make()
  thing.speak()
}
EOF2
check_runs "a value of an undeclared dependency's type" "${project}"

# A build that must fail, with every one of the rest of the arguments in its output.
check_refused() {
    local name="$1" dir="$2"
    shift 2
    if (cd "${dir}" && "${SPP}" build -m dev > "${dir}/refused.log" 2>&1); then
        fail "${name}: the build succeeded" "${dir}/refused.log"
        return
    fi
    tr '\r' '\n' < "${dir}/refused.log" > "${dir}/refused.txt"
    for expected in "$@"; do
        if ! grep -qF "${expected}" "${dir}/refused.txt"; then
            fail "${name}: refused without \"${expected}\"" "${dir}/refused.txt"
            return
        fi
    done
    ok "${name}: refused"
}

# The project writing "b::..." with only "a" declared.
project="${WORK}/undeclared/app"
make_project "${project}"
cat > "${project}/src/main.spp" <<'EOF2'
fun main() -> Void {
  b::api::greet()
}
EOF2
check_refused "the project naming an undeclared dependency" "${project}" \
    "E121" "the dependency 'b'" "this project"

# The same through a type.
cat > "${project}/src/main.spp" <<'EOF2'
fun take(thing: b::api::Thing) -> Void { }

fun main() -> Void { }
EOF2
check_refused "the project naming an undeclared dependency's type" "${project}" "E121"

# Declaring it makes the same code fine, without the fetch being any different.
sed -i "/^a = /a b = { git = \"${REMOTES}/b\", branch = \"master\" }" "${project}/spp.toml"
cat > "${project}/src/main.spp" <<'EOF2'
fun main() -> Void {
  b::api::greet()
}
EOF2
check_runs "the project declaring it too" "${project}"

# "a" naming "b" while only the project declares it.
make_a ""
project="${WORK}/dep-undeclared/app"
make_project "${project}" "b = { git = \"${REMOTES}/b\", branch = \"master\" }"
check_refused "a dependency naming what it does not declare" "${project}" "E121" "the dependency 'a'"
make_a

# A dependency that does not declare std.
nostd="${REMOTES}/nostd"
mkdir -p "${nostd}/src/nostd"
cat > "${nostd}/spp.toml" <<'EOF2'
[project]
name = "nostd"
version = "0.1.0"
build = "lib"
EOF2
touch "${nostd}/src/nostd/empty.spp"
commit_all "${nostd}"
project="${WORK}/no-std-dep/app"
make_project "${project}" "nostd = { git = \"${nostd}\", branch = \"master\" }"
check_refused "a dependency not declaring std" "${project}" "the dependency 'nostd' does not declare 'std'"

# A project that does not.
project="${WORK}/no-std/app"
make_project "${project}"
sed -i '/^std = /d' "${project}/spp.toml"
check_refused "the project not declaring std" "${project}" "this project does not declare 'std'"

# Two dependencies with one top-level namespace.
twin="${REMOTES}/twin"
mkdir -p "${twin}/src/b"
library_toml "${twin}"
touch "${twin}/src/b/other.spp"
commit_all "${twin}"
project="${WORK}/clash-namespace/app"
make_project "${project}" "twin = { git = \"${twin}\", branch = \"master\" }"
check_refused "two dependencies with one namespace" "${project}" \
    "the top-level namespace 'b' is both the dependency 'b' and the dependency 'twin'"

# A library naming the project using it, which it has no way to declare: it would
# not compile for anyone else.
upward="${REMOTES}/upward"
mkdir -p "${upward}/src/upward"
library_toml "${upward}"
cat > "${upward}/src/upward/api.spp" <<'EOF2'
!public
fun f() -> Void {
  mine::x::g()
}
EOF2
commit_all "${upward}"
project="${WORK}/upward/app"
make_project "${project}" "upward = { git = \"${upward}\", branch = \"master\" }"
mkdir -p "${project}/src/mine"
cat > "${project}/src/mine/x.spp" <<'EOF2'
!public
fun g() -> Void { }
EOF2
check_refused "a dependency naming the project" "${project}" "E121" "this project" "the dependency 'upward'"

# "b" declaring "a" back: one visit each, and still a working program, with calls
# going both ways: the project into "a", into "b", back into "a", and into "b".
make_b "a = { git = \"${REMOTES}/a\", branch = \"master\" }"
cat >> "${REMOTES}/b/src/b/api.spp" <<'EOF2'

!public
fun back() -> Void {
  a::api::run()
}
EOF2
commit_all "${REMOTES}/b"
cat >> "${REMOTES}/a/src/a/api.spp" <<'EOF2'

!public
fun through_b() -> Void {
  b::api::back()
}
EOF2
commit_all "${REMOTES}/a"
project="${WORK}/cycle/app"
make_project "${project}"
cat > "${project}/src/main.spp" <<'EOF2'
fun main() -> Void {
  a::api::through_b()
}
EOF2
check_runs "a cycle, with calls both ways" "${project}"

finish
