from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "results"
INPUT_CSV = RESULTS_DIR / "cache_sensitivity.csv"


def plot_metric(frame: pd.DataFrame, metric: str, output_name: str) -> None:
    scenarios = sorted(frame["scenario"].unique())
    schedulers = sorted(frame["scheduler"].unique())
    fig, axes = plt.subplots(
        len(scenarios),
        len(schedulers),
        figsize=(6 * len(schedulers), 4 * len(scenarios)),
        sharex=True,
        sharey=False,
    )

    if len(scenarios) == 1 and len(schedulers) == 1:
        axes = [[axes]]
    elif len(scenarios) == 1:
        axes = [axes]
    elif len(schedulers) == 1:
        axes = [[ax] for ax in axes]

    for row_index, scenario in enumerate(scenarios):
        for col_index, scheduler in enumerate(schedulers):
            ax = axes[row_index][col_index]
            subset = frame[
                (frame["scenario"] == scenario) &
                (frame["scheduler"] == scheduler)
            ].sort_values("override_miss_penalty")

            for policy in sorted(subset["policy"].unique()):
                policy_frame = subset[subset["policy"] == policy]
                ax.plot(
                    policy_frame["override_miss_penalty"],
                    policy_frame[metric],
                    marker="o",
                    label=policy,
                )

            ax.set_title(f"{scenario} / {scheduler.upper()}")
            ax.set_xlabel("Miss penalty override")
            ax.set_ylabel(metric.replace("_", " ").title())
            ax.grid(True, linestyle="--", alpha=0.3)
            if row_index == 0 and col_index == len(schedulers) - 1:
                ax.legend()

    fig.tight_layout()
    fig.savefig(RESULTS_DIR / output_name, dpi=180)
    plt.close(fig)


def main() -> None:
    if not INPUT_CSV.exists():
        raise SystemExit("Run scripts/run_sensitivity.py first.")

    frame = pd.read_csv(INPUT_CSV)
    plot_metric(frame, "crpd_cycles", "cache_sensitivity_crpd.png")
    plot_metric(frame, "deadline_misses", "cache_sensitivity_deadlines.png")
    plot_metric(frame, "max_response_time", "cache_sensitivity_response.png")
    print(f"Saved sensitivity plots into {RESULTS_DIR}")


if __name__ == "__main__":
    main()
