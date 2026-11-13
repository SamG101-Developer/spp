# shellcheck shell=bash
# Run a command that reaches a package server up to three times,
# 10 s then 30 s apart, so a dropped connection does not fail the
# job. A real error (a missing package) fails the same way three
# times, about 40 s later. Bash 3.2 safe: macOS sources it before
# Homebrew's bash is installed.
retry() {
  local attempt=1 status
  local delays=(10 30)
  until "$@"; do
    status=$?
    if [ "$attempt" -gt "${#delays[@]}" ]; then
      echo "::error::'$*' failed ${attempt} times (exit ${status})" >&2
      return "$status"
    fi
    echo "::warning::'$*' failed (exit ${status}), attempt ${attempt}; retrying in ${delays[attempt - 1]}s" >&2
    sleep "${delays[attempt - 1]}"
    attempt=$((attempt + 1))
  done
}
