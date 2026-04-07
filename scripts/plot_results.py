from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "results"
INPUT_CSV = RESULTS_DIR / "experiment_results.csv"


def plot_metric(frame: pd.DataFrame, metric: str, output_name: str) -> None:
    schedulers = sorted(frame["scheduler"].unique())
    fig, axes = plt.subplots(1, len(schedulers), figsize=(8 * len(schedulers), 5), sharey=True)

    if len(schedulers) == 1:
        axes = [axes]

    for ax, scheduler in zip(axes, schedulers):
        pivot = (
            frame[frame["scheduler"] == scheduler]
            .pivot_table(
                index="scenario",
                columns="policy",
                values=metric,
                aggfunc="mean",
            )
            .sort_index()
        )

        pivot.plot(kind="bar", ax=ax, width=0.8)
        ax.set_title(f"{metric.replace('_', ' ').title()} ({scheduler.upper()})")
        ax.set_xlabel("Scenario")
        ax.set_ylabel(metric.replace("_", " ").title())
        ax.grid(axis="y", linestyle="--", alpha=0.4)

    fig.tight_layout()
    fig.savefig(RESULTS_DIR / output_name, dpi=180)
    plt.close(fig)


def main() -> None:
    if not INPUT_CSV.exists():
        raise SystemExit("Run scripts/run_experiments.py first.")

    RESULTS_DIR.mkdir(exist_ok=True)
    frame = pd.read_csv(INPUT_CSV)

    plot_metric(frame, "crpd_cycles", "crpd_cycles.png")
    plot_metric(frame, "deadline_misses", "deadline_misses.png")
    plot_metric(frame, "cross_task_evictions", "cross_task_evictions.png")
    plot_metric(frame, "max_response_time", "max_response_time.png")
    print(f"Saved plots into {RESULTS_DIR}")


if __name__ == "__main__":
    main()
