#!/usr/bin/env python3
"""Query OSV about the pins in .github/dependencies.toml and write SARIF.

osv-scanner reads lockfiles. Nothing in this repository's C++ dependency set is a lockfile: the libraries are git
commits and the prebuilt archives are versions, so osv-scanner never sees them. This closes that gap by asking OSV
directly.

Two mechanisms, both verified against the live API:
  - libraries are matched by commit, which OSV resolves against the GIT ranges in its records
  - a pin carrying osv-name/osv-ecosystem metadata is matched by version

purl-based matching is deliberately not used: pkg:github/<owner>/<repo>@<commit> returns nothing from OSV, so an SBOM
would look like a scan without being one.

Findings are a warning and a SARIF entry; a scan that could not run is a failure. Change the `return 0` at the end of
main() to make findings blocking.
"""

from __future__ import annotations

import datetime
import importlib.util
import json
import sys
import tomllib
import urllib.request
from pathlib import Path

OSV_BATCH = "https://api.osv.dev/v1/querybatch"
OSV_VULN = "https://api.osv.dev/v1/vulns/"
MANIFEST = Path(".github/dependencies.toml")
IGNORES = Path("osv-scanner.toml")
PINS = Path(".github/scripts/lib/pins.py")
TIMEOUT = 30


def load_pins_module():
    """Reuse pins.py so the manifest has exactly one reader."""
    spec = importlib.util.spec_from_file_location("pins", PINS)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def post(url: str, payload: dict) -> dict:
    body = json.dumps(payload).encode()
    request = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=TIMEOUT) as response:  # noqa: S310 - fixed https URL
        return json.load(response)
    return None


def get(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=TIMEOUT) as response:  # noqa: S310 - fixed https URL
        return json.load(response)
    return None


def ignored() -> dict[str, str]:
    """Ignore ids from osv-scanner.toml, the same file osv-scanner itself reads, minus any that have expired."""
    if not IGNORES.is_file():
        return {}
    with IGNORES.open("rb") as handle:
        config = tomllib.load(handle)
    today = datetime.date.today()
    out: dict[str, str] = {}
    for entry in config.get("IgnoredVulns", []):
        until = entry.get("ignoreUntil")
        if isinstance(until, datetime.datetime):
            until = until.date()
        if until and until < today:
            print(f"::warning::the ignore for {entry.get('id')} expired on {until}; re-evaluate it")
            continue
        out[entry["id"]] = entry.get("reason", "(no reason recorded)")
    return out


def targets(data: dict, pins) -> list[dict]:
    """One OSV query per pinned thing, each tagged with where it came from."""
    out = []
    for entry in pins.libraries(data):
        out.append(
            {
                "label": f"{entry['name']} @ {entry['commit'][:12]}",
                "anchor": f'name = "{entry["name"]}"',
                "query": {"commit": entry["commit"]},
            }
        )
    for name, table in data.get("pin", {}).items():
        ecosystem, package = table.get("osv-ecosystem"), table.get("osv-name")
        if not ecosystem or not package:
            continue
        version = table.get("version")
        if not version:
            print(f"::error::pin.{name} has osv metadata but no version to query")
            sys.exit(1)
        out.append(
            {
                "label": f"{package} {version} ({ecosystem})",
                "anchor": f"[pin.{name}]",
                "query": {"package": {"name": package, "ecosystem": ecosystem}, "version": version},
            }
        )
    return out


def line_of(anchor: str) -> int:
    for number, line in enumerate(MANIFEST.read_text().splitlines(), start=1):
        if line.strip() == anchor:
            return number
    return 1


def sarif(findings: list[dict]) -> dict:
    rules = {}
    results = []
    for finding in findings:
        rules.setdefault(
            finding["id"],
            {
                "id": finding["id"],
                "name": finding["id"],
                "shortDescription": {"text": finding["summary"]},
                "helpUri": f"https://osv.dev/vulnerability/{finding['id']}",
                "properties": {"tags": ["security", "supply-chain"]},
            },
        )
        results.append(
            {
                "ruleId": finding["id"],
                "level": "warning",
                "message": {"text": f"{finding['label']}: {finding['summary']} ({finding['id']})"},
                "locations": [
                    {
                        "physicalLocation": {
                            "artifactLocation": {"uri": str(MANIFEST)},
                            "region": {"startLine": finding["line"]},
                        }
                    }
                ],
            }
        )
    return {
        "$schema": "https://json.schemastore.org/sarif-2.1.0.json",
        "version": "2.1.0",
        "runs": [
            {
                "tool": {
                    "driver": {
                        "name": "osv-pinned-deps",
                        "informationUri": "https://osv.dev",
                        "rules": list(rules.values()),
                    }
                },
                "results": results,
            }
        ],
    }


def main() -> int:
    output = Path(sys.argv[1] if len(sys.argv) > 1 else "osv-pins.sarif")
    pins = load_pins_module()
    data = pins.load()
    checks = targets(data, pins)
    skip = ignored()

    try:
        answer = post(OSV_BATCH, {"queries": [c["query"] for c in checks]})
    except (OSError, ValueError) as error:
        print(f"::error::could not reach OSV ({error}); the scan result is unknown, not clean")
        return 1

    results = answer.get("results", [])
    if len(results) != len(checks):
        print(f"::error::OSV returned {len(results)} results for {len(checks)} queries")
        return 1

    findings: list[dict] = []
    muted = 0
    for check, result in zip(checks, results, strict=True):
        for vuln in result.get("vulns", []):
            vid = vuln["id"]
            if vid in skip:
                print(f"  {check['label']}: {vid} ignored - {skip[vid]}")
                muted += 1
                continue
            try:
                summary = get(OSV_VULN + vid).get("summary") or "(no summary)"
            except OSError, ValueError:
                summary = "(details unavailable)"
            findings.append(
                {
                    "id": vid,
                    "label": check["label"],
                    "summary": summary,
                    "line": line_of(check["anchor"]),
                }
            )

    output.write_text(json.dumps(sarif(findings), indent=2) + "\n")
    print(f"queried {len(checks)} pinned dependencies; {len(findings)} findings, {muted} ignored")
    for finding in findings:
        print(
            f"::warning file={MANIFEST},line={finding['line']}::{finding['label']}: {finding['summary']} ({finding['id']})"
        )
    if findings:
        print("::warning::OSV reported vulnerabilities against pinned dependencies; see the Security tab")
    return 0


if __name__ == "__main__":
    sys.exit(main())
