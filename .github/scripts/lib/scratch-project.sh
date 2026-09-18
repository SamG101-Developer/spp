#!/usr/bin/env bash
# Create an empty s++ project under RUNNER_TEMP and leave the
# shell inside it. The compiler's own `project/` tree is not in
# the repository, so a job that needs something to compile has
# to write one, and what `spp init` writes is the standard
# library plus an entry point: the [vcs] section it fills in
# points at the STL, and the build clones it.
#
# `spp init` names the project after its directory, so the name
# has to be a usable module folder - no dashes, no triples. It
# also refuses a directory that is not empty, reporting that on
# stdout while still exiting 0, so the structure it should have
# written is what says whether it ran.
# shellcheck shell=bash

scratch_project() {
  local exe="$1" name="$2"
  local root="${RUNNER_TEMP:-/tmp}/${name}"

  rm -rf "$root"
  mkdir -p "$root"
  cd "$root" || return 1
  "$exe" init

  if [ ! -f spp.toml ] || [ ! -f src/main.spp ]; then
    echo "::error::spp init wrote no project into ${root}"
    return 1
  fi
  echo "project: ${root}"
}
