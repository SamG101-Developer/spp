#!/usr/bin/env bash
# Keep Windows Defender's real-time scan off the workspace and
# temp folders: every gtest starts processes and writes many
# small files there. Reports whether scanning was on, so a run
# shows whether this mattered; an image without Defender is fine.
set -euo pipefail

# The $ names are PowerShell's, not bash's.
# shellcheck disable=SC2016
powershell -NoProfile -NonInteractive -Command '
  $status = Get-MpComputerStatus -ErrorAction SilentlyContinue
  if ($null -eq $status) { Write-Output "defender: not running"; exit 0 }
  Write-Output ("defender: real-time protection " + $status.RealTimeProtectionEnabled)
  if (-not $status.RealTimeProtectionEnabled) { exit 0 }
  Add-MpPreference -ExclusionPath $env:GITHUB_WORKSPACE, $env:RUNNER_TEMP, $env:USERPROFILE -ErrorAction Stop
  Add-MpPreference -ExclusionProcess "spp.exe", "spp_tests.exe", "cl.exe", "clang-cl.exe", "lld-link.exe", "link.exe" -ErrorAction Stop
  Write-Output "defender: excluded the workspace, temp and home folders, and the compiler processes"
'
