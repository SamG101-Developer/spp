#!/usr/bin/env bash

# End-to-end check that "[binary.debug] info" gives a program line tables ("codegen::StartDebugInfo" and friends,
# llvm_debug), with full debug info its variables and their types ("DebugTypeOf", "DeclareVariable"), and the
# source-line table its crash report reads (llvm_lines). Builds a scratch project whose function overflows on a known
# line, at dev and rel, with "info" unset (full at dev, lines at rel), "lines", "full" and "none", and asserts:
#   - the program's object verifies ("llvm-dwarfdump --verify"; the linked executable is not checked, as GNU ld's
#     merging of ".debug_str" leaves offsets into the middle of a string, valid but flagged), and has a line table;
#   - the frame the overflow aborts in resolves ("llvm-symbolizer"), through the inlined checked add, to the
#     overflowing line of main.spp itself (binutils' addr2line stops at the innermost frame of a rel build);
#   - the crash report names, under frame #0, the lines it was inlined through and then "at src/main.spp:13:3", and
#     under frame #1 the line of the call ("at src/main.spp:19:3"), at dev and rel alike;
#   - with full debug info the object describes the variables (DW_TAG_variable), and with line tables alone it does
#     not; and at dev, gdb stopped on the overflowing line shows the parameters and locals with their values: a
#     number, a borrowed class's fields, a tuple, and a variant by the member it holds ("Some[S32]");
#   - with "info = \"none\"" the object has no line table, no source-line table is linked, and the report names no
#     lines.
# Skipped without LLVM 23's llvm-dwarfdump and llvm-symbolizer; the variables are read with gdb where it is installed. The report's lines need a libsppc that reads the table (SPPC_LIB
# replaces the vendored one until SPP-STL ships it).
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-debug-info.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

DWARFDUMP="$(command -v llvm-dwarfdump-23 || echo /usr/lib/llvm-23/bin/llvm-dwarfdump)"
SYMBOLIZER="$(command -v llvm-symbolizer-23 || echo /usr/lib/llvm-23/bin/llvm-symbolizer)"
readonly DWARFDUMP SYMBOLIZER
if [[ ! -x "${DWARFDUMP}" || ! -x "${SYMBOLIZER}" ]]; then
    echo "skipped: no llvm-dwarfdump 23 or llvm-symbolizer 23"
    exit 0
fi

# The overflow is on line 13 of main.spp, in "bump", called on line 19.
read -r -d '' PROGRAM <<'SPP'
use std::hints::black_box
use std::llvm::no_inline

cls Point {
  !public x: S32
  !public y: S32
}

!no_inline
fun bump(p: &Point, x: U8) -> U8 {
  let pair = (x, true)
  let maybe: Opt[S32] = Some(val=p.y)
  let y = x + 200_u8
  ret y
}

fun main() -> Void {
  let p = Point(x=3, y=4)
  let r = bump(&p, black_box(100_u8))
  let s = Str::from(r)
  std::console::println(&s)
  std::mem::ops::drop(s)
  std::mem::ops::drop(p)
}
SPP

# What gdb shows stopped on the overflowing line: the arguments, the locals, the borrowed class, and the variant's
# type; empty without gdb.
gdb_view() {
    local exe="$1"
    command -v gdb > /dev/null || return 0
    (cd "$(dirname "$(dirname "$(dirname "$(dirname "${exe}")")")")" && gdb -batch -nx \
        -ex 'break src/main.spp:13' -ex run -ex 'info args' -ex 'info locals' -ex 'print *p' -ex 'whatis maybe' \
        "${exe}" 2>&1)
}

# The address of the report's frame #0 ("main::bump+0x49"): its symbol's address plus the offset, one byte back into
# the call that failed, as a return address points past it.
frame_address() {
    local exe="$1" log="$2"
    local frame sym off base
    frame="$(grep -m1 -E '^ +#0 ' "${log}" | sed -E 's/.*#0 +([^ ]+)\+0x([0-9a-f]+).*/\1 \2/')"
    sym="${frame% *}" off="${frame#* }"
    base="$(nm "${exe}" | awk -v s="${sym}" '{n = $3; sub(/\$h.*/, "", n); if (n == s) { print $1; exit }}')"
    [[ -n "${base}" ]] && printf '0x%x' $((0x${base} + 0x${off} - 1))
}

check() {
    local mode="$1" info="$2"
    local name="${mode}, info=${info}"
    local dir="${WORK}/${mode}-${info}/lines"
    local config=""
    [[ "${info}" != default ]] && config="
[binary.debug]
info = \"${info}\""
    new_project "${dir}" "${config}"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    # Full debug info: "full" as set, or unset at dev.
    local full=false
    [[ "${info}" == full || ( "${info}" == default && "${mode}" == dev ) ]] && full=true

    local why="" lines variables
    lines="$(readelf -S "${OBJ}" | grep -c '\.debug_line')"
    variables="$("${DWARFDUMP}" --debug-info "${OBJ}" 2>/dev/null | grep -c 'DW_TAG_variable')"
    local table
    table="$(nm "${EXE}" | grep -c ' __spp_lines$')"
    if [[ ${RC} -ne 134 ]] || ! grep -qF "check failed: unsigned add overflowed" "${dir}/run.log"; then
        why="exit ${RC}, not the overflow's abort"
    elif [[ "${info}" == none ]]; then
        if (( lines != 0 )); then
            why="a line table with info = \"none\""
        elif (( table != 0 )); then
            why="a source-line table linked with info = \"none\""
        elif grep -qE '^ +at ' "${dir}/run.log"; then
            why="the report names lines with info = \"none\""
        fi
    elif (( table == 0 )); then
        why="no source-line table linked"
    elif (( lines == 0 )); then
        why="no line table in the object"
    elif ! "${DWARFDUMP}" --verify "${OBJ}" > "${dir}/verify.log" 2>&1; then
        why="the object's DWARF does not verify"
    else
        local address resolved
        address="$(frame_address "${EXE}" "${dir}/run.log")"
        resolved="$("${SYMBOLIZER}" --obj="${EXE}" "${address}")"
        printf '%s\n' "${resolved}" > "${dir}/resolved.log"
        if ! grep -qE '/src/main\.spp:13:3$' <<< "${resolved}"; then
            why="frame #0 does not resolve to main.spp:13:3: ${resolved//$'\n'/ | }"
        elif ! grep -A3 -E '^ +#0 ' "${dir}/run.log" | grep -qE '^ +at src/main\.spp:13:3$'; then
            why="the report does not name src/main.spp:13:3 under frame #0"
        elif ! grep -A3 -E '^ +#0 ' "${dir}/run.log" | grep -qE '^ +at .*\.spp:[0-9]+:[0-9]+  \(inlined '; then
            why="the report does not name the checked add inlined into frame #0"
        elif ! grep -A1 -E '^ +#1 ' "${dir}/run.log" | grep -qE '^ +at src/main\.spp:19:3$'; then
            why="the report does not name src/main.spp:19:3 under frame #1"
        elif [[ "${full}" == true ]] && (( variables == 0 )); then
            why="no variables described with full debug info"
        elif [[ "${full}" == false ]] && (( variables != 0 )); then
            why="variables described with line tables alone"
        fi
    fi

    # The variables as a debugger shows them: at dev, where nothing is optimised away.
    if [[ -z "${why}" && "${mode}" == dev && "${info}" != none ]]; then
        gdb_view "${EXE}" > "${dir}/gdb.log"
        if [[ -s "${dir}/gdb.log" && "${full}" == true ]]; then
            local expected
            # In Rust's forms, as the debug info is marked Rust's: a one-byte integer as a number, a tuple in
            # parentheses, a variant's tag by its enumerator's qualified name.
            for expected in 'x = 100' 'pair = (100, true)' 'tag::std::option::Some[S32], payload: ' '{val: 4}' \
                '= main::Point {x: 3, y: 4}' 'type = std::option::None or std::option::Some[S32]'; do
                grep -qF "${expected}" "${dir}/gdb.log" || { why="gdb does not show \"${expected}\""; break; }
            done
        elif [[ -s "${dir}/gdb.log" ]] && grep -qF 'pair = ' "${dir}/gdb.log"; then
            why="gdb shows variables with line tables alone"
        fi
        [[ -n "${why}" ]] && cp "${dir}/gdb.log" "${dir}/verify.log"
    fi

    if [[ -z "${why}" ]]; then
        ok "${name}"
    else
        fail "${name}: ${why}" "${dir}/verify.log"
    fi
}

for mode in dev rel; do
    check "${mode}" default
    check "${mode}" lines
    check "${mode}" full
    check "${mode}" none
done

finish
