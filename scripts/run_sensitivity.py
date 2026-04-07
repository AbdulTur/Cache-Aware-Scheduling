from __future__ import annotations

import csv
import io
from pathlib import Path

from run_experiments import find_binary, run_command, RESULTS_DIR


OUTPUT_CSV = RESULTS_DIR / "cache_sensitivity.csv"
SCENARIOS = ["sched_compare", "crpd_peak"]
SCHEDULERS = ["rms", "edf"]
POLICIES = ["shared", "colored", "partitioned"]
MISS_PENALTIES = [1, 2, 4, 6, 8]


def run_once(
    scenario: str,
    scheduler: str,
    policy: str,
    miss_penalty: int
) -> dict[str, str]:
    binary = find_binary()
    completed = run_command(binary, scenario, scheduler, policy, ["--miss-penalty", str(miss_penalty)])
    summary_csv = completed.stdout.splitlines()[-2:]
    reader = csv.DictReader(io.StringIO("\n".join(summary_csv)))
    row = next(reader)
    row["override_miss_penalty"] = str(miss_penalty)
    return row


def main() -> None:
    RESULTS_DIR.mkdir(exist_ok=True)
    rows: list[dict[str, str]] = []

    for scenario in SCENARIOS:
        for scheduler in SCHEDULERS:
            for policy in POLICIES:
                for miss_penalty in MISS_PENALTIES:
                    rows.append(run_once(scenario, scheduler, policy, miss_penalty))

    with OUTPUT_CSV.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {len(rows)} sensitivity rows to {OUTPUT_CSV}")


if __name__ == "__main__":
    main()
