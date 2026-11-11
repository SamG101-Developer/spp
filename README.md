# Benchmark records

Written by post_merge.yaml, never by hand. `records/<sha>/<kind>.json`
is the record of each master commit that changed the compiler, from
benchmarks/run.py (`compile`) and benchmarks/runtime.py (`runtime`),
and `latest/<kind>.json` is the newest of each. A pull request
compares against its base commit's records. See
.triages/docs_open/benchmark-ci-plan.md on master.
