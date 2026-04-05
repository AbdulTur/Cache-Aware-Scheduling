from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "results"
INPUT_CSV = RESULTS_DIR / "experiment_results.csv"


def plot_metric(frame: pd.DataFrame, metric: str, output_name: str) -> None:
    pivot = (
        frame.pivot_table(
            index="scenario",
            columns="policy",
            values=metric,
            aggfunc="mean",
        )
        .sort_index()
    )

    fig, ax = plt.subplots(figsize=(8, 5))
    pivot.plot(kind="bar", ax=ax, width=0.8)
    ax.set_title(metric.replace("_", " ").title())
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
    print(f"Saved plots into {RESULTS_DIR}")


if __name__ == "__main__":
    main()
