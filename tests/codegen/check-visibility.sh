#!/usr/bin/env bash

# Visibility across modules, which the unit tests cannot see: each builds one module, and std has no private class. A
# project with a second module ("other::m") declaring a private and a public class names each from "main" in a body, a
# parameter, a return type, a generic argument and an attribute: the private class is an access violation (E83) in
# every one, the public class builds, and the private class builds where it is declared. Signature and attribute types
# are read before a class's visibility reaches its symbol, so those checks run once every module has it (open bug 45).
#
#   tests/codegen/check-visibility.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' OTHER <<'SPP'
cls Hidden {
    a: U64
}

sup Hidden {
    fun keep(h: Hidden) -> Hidden { ret h }
}

fun local(h: Hidden) -> Hidden { ret h }

!public
cls Shown {
    !public a: U64
}
SPP

# One case: "main" built beside "other::m", expected to build ("ok") or to fail with E83 ("e83").
check() {
    local name="$1" want="$2" main="$3"
    local dir="${WORK}/vis-${RANDOM}/vis"
    new_project "${dir}"
    mkdir -p "${dir}/src/other"
    printf '%s\n' "${OTHER}" > "${dir}/src/other/m.spp"
    printf '%s\n' "${main}" > "${dir}/src/main.spp"

    (cd "${dir}" && "${SPP}" build -m dev > "${dir}/build.log" 2>&1)
    local rc=$?
    tr '\r' '\n' < "${dir}/build.log" | sed 's/\x1b\[[0-9;]*m//g' > "${dir}/plain.log"
    if [[ "${want}" == ok ]] && (( rc == 0 )); then
        ok "${name}"
    elif [[ "${want}" == e83 ]] && (( rc != 0 )) && grep -q '^E83:' "${dir}/plain.log"; then
        ok "${name}"
    else
        fail "${name}: wanted ${want}, build exited ${rc}" "${dir}/plain.log"
    fi
}

check "private class in a body" e83 'fun main() -> Void {
    let t = other::m::Hidden(a=1_u64)
    std::mem::ops::drop(t)
}'

check "private class as a parameter type" e83 'fun f(t: other::m::Hidden) -> Void { std::mem::ops::drop(t) }
fun main() -> Void { }'

check "private class as a return type" e83 'fun f() -> other::m::Hidden { ret other::m::Hidden(a=1_u64) }
fun main() -> Void { }'

check "private class as a generic argument" e83 'fun f(v: std::vector::Vec[other::m::Hidden]) -> Void {
    std::mem::ops::drop(v)
}
fun main() -> Void { }'

check "private class as an attribute type" e83 'cls Holder {
    h: other::m::Hidden
}
fun main() -> Void { }'

check "public class as a parameter type" ok 'fun f(t: other::m::Shown) -> Void { std::mem::ops::drop(t) }
fun main() -> Void { }'

check "public class as an attribute type" ok 'cls Holder {
    h: other::m::Shown
}
fun main() -> Void { }'

check "private class used where it is declared" ok 'fun main() -> Void { }'

finish
