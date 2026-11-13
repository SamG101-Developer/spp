#!/usr/bin/env bash
# platforms: linux macos windows

# End-to-end check of "spp self install", "spp self update" and "spp self uninstall" (cli_self.cpp), on Linux, macOS
# and Windows (Git Bash). The update runs against a local release server laid out as GitHub's (SPP_UPDATE_BASE_URL):
# "releases/latest" redirecting to "releases/tag/v<latest>", and each tag's assets under "releases/download/v<version>/"
# (spp-<label>.zip, packed by the release pipeline's pack.py, and SHA256SUMS). A release's spp is a stand-in: a copy of
# this spp with marker bytes after it, which runs the same and is told apart by its bytes. A copy of spp is updated in
# a scratch directory, and asserts:
#   - with the latest release the same version as it, it says so and is left as it is;
#   - with a newer one, it is replaced by the zip's spp, which runs, and nothing is left beside it (on Windows, the old
#     one moved aside as spp.exe.old, gone once the new one has run);
#   - with a zip that does not match SHA256SUMS, it refuses and is left as it is;
#   - in a directory it cannot write to, it says so and is left as it is (not on Windows, where chmod cannot deny it);
#   - with an older latest release, it says it is newer and is left as it is;
#   - with "--version", the release named is installed whatever the latest is, and one before 0.7.0, one with no
#     release, the running one and a malformed one are each refused (or said to be current) and change nothing;
#   - "spp self uninstall" removes the copy only once confirmed: with no answer, or "n", it is kept; with "y", or
#     "--yes", it is removed (on Windows, moved aside and deleted once it has exited) and the projects' "vcs" and "out"
#     named as left; in a directory it cannot write to, it is refused and kept (not on Windows);
#   - "spp self install", in a scratch home (HOME, or LOCALAPPDATA on Windows) with a controlled PATH, copies spp to
#     its bin and puts that on PATH: on Linux and macOS through ~/.spp/env, one line in each shell start-up file there
#     and a fish snippet, so a new shell finds it; on Windows in the user Path (saved first, and restored after). Again,
#     it refuses as installed, and with --force installs without a second line; with another spp on PATH it refuses,
#     naming it; --dir with --no-modify-path touches neither; "spp self uninstall" of a stray copy leaves that setup,
#     and of the installed one removes the binary and the setup.
# Skipped without curl or Python (and unzip on Linux), or on a platform with no release; under CI's
# SPP_CI_REQUIRE_TOOLS a missing tool fails.
#
#   tests/codegen/check-self.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

case "$(uname -s)" in
    Linux) PLATFORM=linux EXE="" ;;
    Darwin) PLATFORM=macos EXE="" ;;
    MINGW* | MSYS* | CYGWIN*) PLATFORM=windows EXE=".exe" ;;
    *) echo "skipped: $(uname -s) is not a platform spp releases for"; exit 0 ;;
esac
case "${PLATFORM}-$(uname -m)" in
    linux-x86_64) LABEL=linux-x64 ;;
    linux-aarch64) LABEL=linux-arm64 ;;
    macos-arm64) LABEL=macos-arm64 ;;
    windows-x86_64) LABEL=windows-x64 ;;
    *) echo "skipped: no release is built for ${PLATFORM}-$(uname -m)"; exit 0 ;;
esac
readonly PLATFORM EXE LABEL

PY="$(command -v python3 || command -v python || echo python3)"
readonly PY
need_tool curl
need_tool "${PY}"
[[ "${PLATFORM}" == linux ]] && need_tool unzip

# A path as a Windows program reads it ("C:\..."); itself elsewhere.
native() {
    if [[ "${PLATFORM}" == windows ]]; then cygpath -w "$1"; else printf '%s\n' "$1"; fi
}

CURRENT="$("${SPP}" version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
readonly CURRENT
SERVED="${WORK}/served"
INSTALL="${WORK}/install"
mkdir -p "${SERVED}" "${INSTALL}"

# The stand-in spp a release of "version" holds: this spp, then "spp-stand-in <version>", re-signed on macOS, where
# the bytes after it break the signature that arm64 requires.
stand_in() { printf '%s\n' "${WORK}/stage-$1/spp${EXE}"; }

# A release of "version" (zip and SHA256SUMS), made the latest; "corrupt" lists a wrong sha256.
publish() {
    local version="$1" corrupt="${2:-}"
    local tag="${SERVED}/releases/download/v${version}" binary
    binary="$(stand_in "${version}")"
    rm -rf "${tag}" "$(dirname "${binary}")"
    mkdir -p "${tag}" "$(dirname "${binary}")"
    cp "${SPP}" "${binary}"
    printf 'spp-stand-in %s' "${version}" >> "${binary}"
    [[ "${PLATFORM}" == macos ]] && codesign --force --sign - "${binary}" > /dev/null 2>&1
    "${PY}" "$(native "${ROOT}/.github/scripts/release/pack.py")" "$(native "${tag}/spp-${LABEL}.zip")" \
        "$(native "${binary}")" > /dev/null
    "${PY}" - "$(native "${tag}")" "spp-${LABEL}.zip" "${corrupt}" << 'PY'
import hashlib, pathlib, sys
tag, name, corrupt = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
digest = hashlib.sha256((tag / name).read_bytes()).hexdigest()
if corrupt:
    digest = "0" * 8 + digest[8:]
(tag / "SHA256SUMS").write_bytes(f"{digest}  {name}\n".encode())
PY
    echo "${version}" > "${SERVED}/latest"
}

# The server: GitHub's redirect for "latest" (re-read from SERVED/latest on each request), the files under it.
cat > "${WORK}/server.py" << 'PY'
import http.server, os, sys
root = sys.argv[1]
class Releases(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=root, **kwargs)
    def redirect_latest(self):
        if self.path.rstrip("/") != "/releases/latest":
            return False
        with open(os.path.join(root, "latest")) as f:
            latest = f.read().strip()
        self.send_response(302)
        self.send_header("Location", f"/releases/tag/v{latest}")
        self.end_headers()
        return True
    def tag_page(self):
        # A tag's page exists when the tag has a release, as GitHub's does.
        if not self.path.startswith("/releases/tag/"):
            return False
        tag = self.path[len("/releases/tag/"):].strip("/")
        self.send_response(200 if os.path.isdir(os.path.join(root, "releases", "download", tag)) else 404)
        self.send_header("Content-Length", "0")
        self.end_headers()
        return True
    def do_HEAD(self):
        if not (self.redirect_latest() or self.tag_page()):
            super().do_HEAD()
    def do_GET(self):
        if not (self.redirect_latest() or self.tag_page()):
            super().do_GET()
    def log_message(self, *args):
        pass
server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Releases)
with open(sys.argv[2], "w") as f:
    f.write(str(server.server_address[1]))
server.serve_forever()
PY
"${PY}" "$(native "${WORK}/server.py")" "$(native "${SERVED}")" "$(native "${WORK}/port")" &
SERVER_PID=$!

# On exit: the server stopped, the user Path put back (Windows), the scratch directory removed.
cleanup() {
    kill "${SERVER_PID}" 2> /dev/null
    [[ "${SAVED:-}" == 1 ]] && user_path_set "${SAVED_USER_PATH}"
    rm -rf "${WORK}"
}
trap cleanup EXIT
for _ in $(seq 1 100); do [[ -s "${WORK}/port" ]] && break; sleep 0.1; done
PORT="$(cat "${WORK}/port")"
export SPP_UPDATE_BASE_URL="http://127.0.0.1:${PORT}"

# A fresh copy of this spp to update, run from where it is.
install_copy() {
    chmod -R u+w "${INSTALL}"
    rm -rf "${INSTALL:?}"/*
    cp "${SPP}" "${INSTALL}/spp${EXE}"
}

# "spp self update" from the copy, with any further arguments, its output in "log", its exit status in RC.
update() {
    local log="$1"
    shift
    "${INSTALL}/spp${EXE}" self update "$@" > "${log}" 2>&1
    RC=$?
}

# Whether the copy is still this spp, byte for byte.
unchanged() { cmp -s "${SPP}" "${INSTALL}/spp${EXE}"; }

# Whether the copy is now the stand-in of "version", and runs.
replaced_by() { cmp -s "$(stand_in "$1")" "${INSTALL}/spp${EXE}" && "${INSTALL}/spp${EXE}" version > /dev/null 2>&1; }

# What is in the install directory besides the copy, one name a line.
beside() { (cd "${INSTALL}" && find . -mindepth 1 ! -name "spp${EXE}" | sed 's|^\./||'); }

# Wait (10 s at most) for "path" to be gone: on Windows a removed spp.exe.old is deleted once nothing runs it.
gone_soon() {
    local path="$1"
    for _ in $(seq 1 100); do [[ -e "${path}" ]] || return 0; sleep 0.1; done
    return 1
}

install_copy
publish "${CURRENT}"
update "${WORK}/same.log"
if (( RC != 0 )) || ! grep -qF "is the latest release" "${WORK}/same.log"; then
    fail "the latest release: exit ${RC}, not said to be the latest" "${WORK}/same.log"
elif ! unchanged; then
    fail "the latest release: the binary changed"
else
    ok "the latest release (${CURRENT}): said so, left as it is"
fi

publish 999.0.0
update "${WORK}/newer.log"
left="$(beside)"
if (( RC != 0 )); then
    fail "a newer release: exit ${RC}" "${WORK}/newer.log"
elif ! replaced_by 999.0.0; then
    fail "a newer release: the binary in place is not the release's, or does not run" "${WORK}/newer.log"
elif [[ "${PLATFORM}" != windows && -n "${left}" ]]; then
    fail "a newer release: left beside it: ${left//$'\n'/ }"
elif [[ "${PLATFORM}" == windows && "${left}" != "spp.exe.old" && -n "${left}" ]]; then
    fail "a newer release: left beside it: ${left//$'\n'/ }"
elif [[ "${PLATFORM}" == windows && -e "${INSTALL}/spp.exe.old" ]]; then
    fail "a newer release: spp.exe.old still there after the new spp ran"
else
    ok "a newer release (999.0.0): replaced, and the new one runs"
fi

install_copy
publish 999.0.1 corrupt
update "${WORK}/corrupt.log"
if (( RC == 0 )) || ! grep -qF "does not match the release's SHA256SUMS" "${WORK}/corrupt.log"; then
    fail "a corrupt download: exit ${RC}, not refused for its checksum" "${WORK}/corrupt.log"
elif ! unchanged; then
    fail "a corrupt download: the binary changed"
else
    ok "a corrupt download: refused, left as it is"
fi

if [[ "${PLATFORM}" == windows ]]; then
    echo "skip  an unwritable install: chmod cannot deny writing on Windows"
elif [[ "$(id -u)" == 0 ]]; then
    echo "skip  an unwritable install: running as root, which writes anywhere"
else
    install_copy
    publish 999.0.2
    chmod a-w "${INSTALL}"
    update "${WORK}/readonly.log"
    chmod u+w "${INSTALL}"
    if (( RC == 0 )) || ! grep -qF "cannot write to" "${WORK}/readonly.log"; then
        fail "an unwritable install: exit ${RC}, not refused for its directory" "${WORK}/readonly.log"
    elif ! unchanged; then
        fail "an unwritable install: the binary changed"
    else
        ok "an unwritable install: refused, left as it is"
    fi
fi

install_copy
publish 0.0.1
update "${WORK}/older.log"
if (( RC != 0 )) || ! grep -qF "is newer than the latest release" "${WORK}/older.log"; then
    fail "an older latest release: exit ${RC}, not said to be newer" "${WORK}/older.log"
elif ! unchanged; then
    fail "an older latest release: the binary changed"
else
    ok "an older latest release (0.0.1): said this one is newer, left as it is"
fi

# "--version": the release named, not the latest.
install_copy
publish 999.1.0
publish "${CURRENT}"
update "${WORK}/named.log" --version 999.1.0
if (( RC != 0 )); then
    fail "--version 999.1.0: exit ${RC}" "${WORK}/named.log"
elif ! replaced_by 999.1.0; then
    fail "--version 999.1.0: the binary in place is not that release's" "${WORK}/named.log"
else
    ok "--version 999.1.0, with ${CURRENT} the latest: that release installed"
fi

# Each refused (or already current), the binary left as it is.
for entry in "0.6.0|predates 0.7.0" "888.0.0|there is no release 888.0.0" "${CURRENT}|already the version asked for" \
    "abc|is not a version"; do
    asked="${entry%%|*}" said="${entry#*|}"
    install_copy
    update "${WORK}/asked.log" --version "${asked}"
    if ! grep -qF "${said}" "${WORK}/asked.log"; then
        fail "--version ${asked}: does not say \"${said}\"" "${WORK}/asked.log"
    elif ! unchanged; then
        fail "--version ${asked}: the binary changed"
    elif [[ "${asked}" == "${CURRENT}" ]] && (( RC != 0 )); then
        fail "--version ${asked}: exit ${RC}" "${WORK}/asked.log"
    elif [[ "${asked}" != "${CURRENT}" ]] && (( RC == 0 )); then
        fail "--version ${asked}: exit 0, not refused" "${WORK}/asked.log"
    else
        ok "--version ${asked}: \"${said}\", left as it is"
    fi
done

# "spp self uninstall": removed only once confirmed.
for entry in "none|" "no|n" "yes|y" "flag|"; do
    name="${entry%%|*}" answer="${entry#*|}"
    install_copy
    if [[ "${name}" == flag ]]; then
        "${INSTALL}/spp${EXE}" self uninstall --yes > "${WORK}/uninstall.log" 2>&1 < /dev/null
    elif [[ "${name}" == none ]]; then
        "${INSTALL}/spp${EXE}" self uninstall > "${WORK}/uninstall.log" 2>&1 < /dev/null
    else
        "${INSTALL}/spp${EXE}" self uninstall > "${WORK}/uninstall.log" 2>&1 <<< "${answer}"
    fi
    RC=$?
    case "${name}" in
        none | no)
            if (( RC == 0 )) || ! grep -qF "Nothing was removed" "${WORK}/uninstall.log" || ! unchanged; then
                fail "uninstall, answered '${answer}': not kept" "${WORK}/uninstall.log"
            else
                ok "uninstall, answered '${answer}': kept"
            fi
            ;;
        *)
            if (( RC != 0 )) || [[ -e "${INSTALL}/spp${EXE}" ]]; then
                fail "uninstall (${name}): exit ${RC}, not removed" "${WORK}/uninstall.log"
            elif ! grep -qF "'vcs' and 'out' folders" "${WORK}/uninstall.log"; then
                fail "uninstall (${name}): removed, without saying what is left" "${WORK}/uninstall.log"
            elif ! gone_soon "${INSTALL}/spp.exe.old"; then
                fail "uninstall (${name}): spp.exe.old still there 10 s after" "${WORK}/uninstall.log"
            else
                ok "uninstall (${name}): removed, and what is left named"
            fi
            ;;
    esac
done

if [[ "${PLATFORM}" != windows && "$(id -u)" != 0 ]]; then
    install_copy
    chmod a-w "${INSTALL}"
    "${INSTALL}/spp${EXE}" self uninstall --yes > "${WORK}/uninstall.log" 2>&1
    RC=$?
    chmod u+w "${INSTALL}"
    if (( RC == 0 )) || ! grep -qF "could not remove" "${WORK}/uninstall.log" || ! unchanged; then
        fail "uninstall, an unwritable install: not refused and kept" "${WORK}/uninstall.log"
    else
        ok "uninstall, an unwritable install: refused, kept"
    fi
fi

# "spp self install", in a scratch home: never the real HOME, LOCALAPPDATA or PATH, and on Windows the user Path is
# saved here and put back on exit. On Linux and macOS the home is reached through a symbolic link, so the path the
# setup records differs from the installed spp's own (resolved) one, as macOS's /var -> /private/var makes it.
HOME_DIR="${WORK}/home"
if [[ "${PLATFORM}" != windows ]]; then
    mkdir -p "${WORK}/home-real"
    ln -s home-real "${HOME_DIR}"
fi
mkdir -p "${HOME_DIR}/.config/fish"
printf 'export EDITOR=vi\n' > "${HOME_DIR}/.bashrc"
# shellcheck disable=SC2016 # the line as written into .bashrc, $HOME and all
LINE='. "$HOME/.spp/env" # added by "spp self install"'
if [[ "${PLATFORM}" == windows ]]; then
    BIN_DIR="${HOME_DIR}/spp/bin"
    SYSTEM="$(cygpath -u "${SYSTEMROOT:-C:\\Windows}")/System32"
    BASE_PATH="/usr/bin:/bin:${SYSTEM}:${SYSTEM}/WindowsPowerShell/v1.0"
else
    BIN_DIR="${HOME_DIR}/.spp/bin"
    BASE_PATH="/usr/bin:/bin"
fi
INSTALLED="${BIN_DIR}/spp${EXE}"

# The user Path, as stored (entries such as %USERPROFILE% unexpanded), read and written through PowerShell.
cat > "${WORK}/user-path.ps1" << 'PS1'
param([string]$Mode, [string]$File)
$key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment', $true)
if ($Mode -eq 'get') {
    $key.GetValue('Path', '', [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
} else {
    $key.SetValue('Path', [IO.File]::ReadAllText($File), [Microsoft.Win32.RegistryValueKind]::ExpandString)
}
PS1
user_path_get() {
    powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$(native "${WORK}/user-path.ps1")" get \
        | tr -d '\r'
}
user_path_set() {
    printf '%s' "$1" > "${WORK}/user-path.txt"
    powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$(native "${WORK}/user-path.ps1")" set \
        "$(native "${WORK}/user-path.txt")"
}

# Whether "dir" is in the user Path (Windows), comparing as Windows does, without regard to case.
in_user_path() {
    local want entry
    want="$(native "$1")"
    while IFS= read -r entry; do
        [[ "${entry,,}" == "${want,,}" ]] && return 0
    done < <(user_path_get | tr ';' '\n')
    return 1
}

# Whether the PATH setup for the installed spp is in place: the user Path holds its directory (Windows); ~/.spp/env
# exists, .bashrc holds its line once beside its own, and a fish snippet is there (elsewhere).
setup_in_place() {
    if [[ "${PLATFORM}" == windows ]]; then
        in_user_path "${BIN_DIR}"
    else
        [[ -f "${HOME_DIR}/.spp/env" && "$(grep -cxF "${LINE}" "${HOME_DIR}/.bashrc")" == 1 ]] \
            && grep -qxF 'export EDITOR=vi' "${HOME_DIR}/.bashrc" \
            && [[ -f "${HOME_DIR}/.config/fish/conf.d/spp.fish" ]]
    fi
}

# Run a copy of spp ("from") as "spp self <args>", in the scratch home, with PATH "path".
self_in_home() {
    local log="$1" from="$2" path="$3"
    shift 3
    if [[ "${PLATFORM}" == windows ]]; then
        env LOCALAPPDATA="$(native "${HOME_DIR}")" PATH="${path}" "${from}" self "$@" > "${log}" 2>&1 < /dev/null
    else
        env HOME="${HOME_DIR}" PATH="${path}" "${from}" self "$@" > "${log}" 2>&1 < /dev/null
    fi
    RC=$?
}

if [[ "${PLATFORM}" == windows ]]; then
    SAVED_USER_PATH="$(user_path_get)"
    SAVED=1
fi

install_copy
self_in_home "${WORK}/install.log" "${INSTALL}/spp${EXE}" "${BASE_PATH}" install
found="${INSTALLED}"
if [[ "${PLATFORM}" != windows ]]; then
    # shellcheck disable=SC2016 # $HOME is the new shell's
    found="$(env -i HOME="${HOME_DIR}" PATH="${BASE_PATH}" sh -c '. "$HOME/.spp/env" && command -v spp')"
fi
if (( RC != 0 )) || ! cmp -s "${SPP}" "${INSTALLED}"; then
    fail "install: exit ${RC}, not copied to ${INSTALLED}" "${WORK}/install.log"
elif ! setup_in_place; then
    fail "install: its PATH setup is not in place" "${WORK}/install.log"
elif [[ "${found}" != "${INSTALLED}" ]]; then
    fail "install: a new shell finds spp at '${found}', not ${INSTALLED}" "${WORK}/install.log"
else
    ok "install: copied, and on PATH for new shells"
fi

self_in_home "${WORK}/again.log" "${INSTALLED}" "${BASE_PATH}" install
if (( RC == 0 )) || ! grep -qF "already installed" "${WORK}/again.log"; then
    fail "install again: not refused as installed" "${WORK}/again.log"
else
    ok "install again: refused, already installed"
fi

self_in_home "${WORK}/force.log" "${INSTALL}/spp${EXE}" "${BASE_PATH}" install --force
if (( RC != 0 )) || ! setup_in_place; then
    fail "install --force: exit ${RC}, or its setup not in place exactly once" "${WORK}/force.log"
else
    ok "install --force: installed again, its setup there once"
fi

mkdir -p "${WORK}/other"
cp "${SPP}" "${WORK}/other/spp${EXE}"
rm "${INSTALLED}"
self_in_home "${WORK}/shadow.log" "${INSTALL}/spp${EXE}" "${WORK}/other:${BASE_PATH}" install
if (( RC == 0 )) || ! grep -qF "another spp is on PATH, at $(native "${WORK}/other/spp${EXE}")" "${WORK}/shadow.log"
then
    fail "install with another spp on PATH: not refused, naming it" "${WORK}/shadow.log"
elif [[ -e "${INSTALLED}" ]]; then
    fail "install with another spp on PATH: refused, but installed anyway"
else
    ok "install with another spp on PATH: refused, naming it"
fi
self_in_home "${WORK}/install.log" "${INSTALL}/spp${EXE}" "${BASE_PATH}" install

cp "${HOME_DIR}/.bashrc" "${WORK}/bashrc.before"
[[ "${PLATFORM}" == windows ]] && user_path_get > "${WORK}/user-path.before"
self_in_home "${WORK}/dir.log" "${INSTALL}/spp${EXE}" "${BASE_PATH}" install --dir "$(native "${WORK}/elsewhere")" \
    --no-modify-path
if (( RC != 0 )) || ! cmp -s "${SPP}" "${WORK}/elsewhere/spp${EXE}"; then
    fail "install --dir --no-modify-path: exit ${RC}, not copied there" "${WORK}/dir.log"
elif ! cmp -s "${WORK}/bashrc.before" "${HOME_DIR}/.bashrc"; then
    fail "install --dir --no-modify-path: .bashrc changed" "${WORK}/dir.log"
elif [[ "${PLATFORM}" == windows ]] && [[ "$(user_path_get)" != "$(cat "${WORK}/user-path.before")" ]]; then
    fail "install --dir --no-modify-path: the user Path changed" "${WORK}/dir.log"
else
    ok "install --dir --no-modify-path: copied there, PATH setup untouched"
fi

self_in_home "${WORK}/stray.log" "${WORK}/elsewhere/spp${EXE}" "${BASE_PATH}" uninstall --yes
if (( RC != 0 )) || ! setup_in_place; then
    fail "uninstall of another copy: the installed one's setup was touched" "${WORK}/stray.log"
else
    ok "uninstall of another copy: the installed one's setup left"
fi

self_in_home "${WORK}/remove.log" "${INSTALLED}" "${BASE_PATH}" uninstall --yes
if (( RC != 0 )) || [[ -e "${INSTALLED}" ]] || ! gone_soon "${INSTALLED}.old"; then
    fail "uninstall of the installed one: exit ${RC}, or not removed" "${WORK}/remove.log"
elif [[ "${PLATFORM}" == windows ]] && in_user_path "${BIN_DIR}"; then
    fail "uninstall of the installed one: its directory is still in the user Path" "${WORK}/remove.log"
elif [[ "${PLATFORM}" != windows && -e "${HOME_DIR}/.spp" ]]; then
    fail "uninstall of the installed one: ~/.spp is still there" "${WORK}/remove.log"
elif [[ "${PLATFORM}" != windows ]] && { grep -qxF "${LINE}" "${HOME_DIR}/.bashrc" \
    || ! grep -qxF 'export EDITOR=vi' "${HOME_DIR}/.bashrc" || [[ -e "${HOME_DIR}/.config/fish/conf.d/spp.fish" ]]; }
then
    fail "uninstall of the installed one: its setup is not all taken back" "${WORK}/remove.log"
else
    ok "uninstall of the installed one: binary and setup removed, start-up files' own lines kept"
fi

finish
