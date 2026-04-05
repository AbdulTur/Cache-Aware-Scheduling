from __future__ import annotations

import csv
import io
import os
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build"
RESULTS_DIR = ROOT / "results"
OUTPUT_CSV = RESULTS_DIR / "experiment_results.csv"

SCENARIOS = ["demo", "harmonic", "stress"]
SCHEDULERS = ["rms", "edf"]
POLICIES = ["shared", "partitioned", "colored"]


def find_binary() -> Path:
    candidates = [
        BUILD_DIR / "cache_aware_scheduler",
        BUILD_DIR / "cache_aware_scheduler.exe",
        BUILD_DIR / "Release" / "cache_aware_scheduler.exe",
        BUILD_DIR / "Release" / "cache_aware_scheduler",
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    suffix = ".exe" if os.name == "nt" else ""
    raise SystemExit(
        "Build the simulator first. Expected binary like "
        f"`build/cache_aware_scheduler{suffix}` or `build/Release/cache_aware_scheduler{suffix}`."
    )


def run_once(scenario: str, scheduler: str, policy: str) -> dict[str, str]:
    binary = find_binary()
    command = [
        str(binary),
        "--scenario",
        scenario,
        "--scheduler",
        scheduler,
        "--policy",
        policy,
        "--summary-csv",
        "-",
    ]
    completed = subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )

    summary_csv = completed.stdout.splitlines()[-2:]
    reader = csv.DictReader(io.StringIO("\n".join(summary_csv)))
    return next(reader)


def main() -> None:
    RESULTS_DIR.mkdir(exist_ok=True)
    rows: list[dict[str, str]] = []

    for scenario in SCENARIOS:
        for scheduler in SCHEDULERS:
            for policy in POLICIES:
                rows.append(run_once(scenario, scheduler, policy))

    with OUTPUT_CSV.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {len(rows)} experiment rows to {OUTPUT_CSV}")


if __name__ == "__main__":
    main()
