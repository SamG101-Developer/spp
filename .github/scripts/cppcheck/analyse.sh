#!/usr/bin/env bash
# Run cppcheck over the sources, annotate the findings and mirror the report into the job summary. Note: there is
# deliberately no -e, as the report has to be published even on findings.
set -uo pipefail

# Confirm the cppcheck binary is accessible and working properly.
export PATH="$SPP_CPPCHECK_PREFIX/bin:$PATH"
cppcheck --version

# Use a custom matcher json configuration to turn each reported line into an inline PR annotation. This cleans up the
# entire report.
echo "::add-matcher::.github/matchers/cppcheck.json"

# The flags "--error-exitcode=1" with "--enable=all" makes every severity a hard failure. All configuration flags are
# registered here to maximise the strictness of the checks (excluding the checks in the .cppcheck-suppressions file).
cppcheck \
  --enable=all \
  --inline-suppr \
  --suppressions-list=.cppcheck-suppressions \
  --check-level=exhaustive \
  --std="$STD" \
  --language=c++ \
  --error-exitcode=1 \
  --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
  --template-location='{file}:{line}:{column}: note: {info}' \
  --output-file=cppcheck-report.txt \
  -j "$(nproc)" \
  -I headers \
  headers sources main.cpp
status=$?

# Remove the registered matcher for the cleanup phase of cppcheck.
echo "::remove-matcher owner=cppcheck::"

# Add the start of the report file into the github summary for easy access. Prepare the summary configs as github limits
# the amount of data showable in the inline report.
summary_limit=60000

if [ -s cppcheck-report.txt ]; then
  report_size=$(wc -c < cppcheck-report.txt)
  {
    echo "### Cppcheck findings"
    echo '```'
    head -c "$summary_limit" cppcheck-report.txt

    # The truncated tail has no trailing newline, so terminate the line before the closing triple-backtick is added,
    # which ends the code-formatted output report.
    [ "$report_size" -gt "$summary_limit" ] && echo
    echo '```'

    # Add the truncation warning message if the true report size is greater than the summary limit on github. Point
    # towards the artefact.
    if [ "$report_size" -gt "$summary_limit" ]; then
      echo "_Truncated: showing the first $summary_limit of $report_size bytes. Download the \`cppcheck-report\` artefact for the full report._"
    fi
  } >> "$GITHUB_STEP_SUMMARY"
fi

# Lift the status of the cppcheck run as the exit code of this script, allowing the action to detect failure or pass,
# for the github workflow reporting.
exit "$status"
