#!/usr/bin/env python3
"""
heatmap_from_csv.py

Reads a CSV where:
  - Each LINE = one time step (configurable period, default 1 second)
  - Each comma-separated VALUE = memory accesses for a 2MB page at that time

Builds a matrix shaped [num_pages x num_time_steps] and plots a heatmap.
"""

import argparse
import csv
import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def read_csv_as_columns(path: Path, delimiter: str = ",", allow_ragged: bool = False) -> np.ndarray:
    cols = []
    max_len = 0

    with path.open("r", newline="") as f:
        reader = csv.reader(f, delimiter=delimiter)
        for i, row in enumerate(reader):
            if not row:
                cols.append([])
                continue
            try:
                values = [float(x) for x in row]
            except ValueError as e:
                raise ValueError(f"Non-numeric value on line {i+1}: {row}") from e
            cols.append(values)
            max_len = max(max_len, len(values))

    if not cols:
        raise ValueError("CSV appears to be empty.")

    if not allow_ragged:
        lengths = {len(c) for c in cols}
        if len(lengths) != 1:
            raise ValueError(
                f"CSV has ragged rows: {sorted(lengths)}. Use --allow-ragged to pad."
            )
        matrix = np.array(cols, dtype=float).T
    else:
        padded = []
        for c in cols:
            if len(c) < max_len:
                c = c + [0.0] * (max_len - len(c))
            padded.append(c)
        matrix = np.array(padded, dtype=float).T

    return matrix


def plot_heatmap(data: np.ndarray, title: str, out_path: Path | None, show: bool,
                 vmin: float | None, vmax: float | None, cmap: str, dpi: int,
                 time_period_ms: int):
    plt.figure(figsize=(10, 6), dpi=dpi)
    im = plt.imshow(
        data,
        origin="lower",
        aspect="auto",
        interpolation="nearest",
        cmap=cmap,
        vmin=vmin,
        vmax=vmax,
    )
    cbar = plt.colorbar(im)
    cbar.set_label("Transformed memory accesses per 2MB page", rotation=90)

    num_pages, num_time_steps = data.shape

    # Always use seconds as the time unit for display
    time_unit = "s"
    time_value = time_period_ms / 1000.0  # Convert ms to seconds
    total_duration = num_time_steps * time_value  # Total elapsed time in seconds

    if time_value == 1:
        time_label = f"Time ({time_unit})"
        time_title = f"({num_pages} pages × {total_duration} {time_unit} total)"
    else:
        time_label = f"Time ({time_unit})"
        time_title = f"({num_pages} pages × {total_duration} {time_unit} total, {time_value} {time_unit} per step)"

    plt.xlabel(time_label)
    plt.ylabel("2MB Page Index")
    plt.title(title if title else f"Memory Access Heatmap {time_title}")

    # Set x-axis ticks to show elapsed time rather than time step indices
    if num_time_steps > 0:
        # Create ticks at regular intervals showing elapsed time
        tick_interval = max(1, num_time_steps // 10)  # Show about 10 ticks max
        tick_positions = np.arange(0, num_time_steps, tick_interval)

        # Format tick labels based on whether time_value is a whole number
        if time_value == int(time_value):
            # Whole number seconds per step - use integer formatting
            tick_labels = [f"{int(pos * time_value)}" for pos in tick_positions]
        else:
            # Fractional seconds per step - use decimal formatting
            tick_labels = [f"{pos * time_value:.1f}" for pos in tick_positions]

        plt.xticks(tick_positions, tick_labels)
    plt.tight_layout()

    if out_path:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(out_path, bbox_inches="tight")
        print(f"Saved heatmap to: {out_path}")

    if show or not out_path:
        plt.show()
    plt.close()


def main():
    p = argparse.ArgumentParser(description="Plot a memory access heatmap from a CSV (lines = time steps, values = pages).")
    p.add_argument("csv", type=Path, help="Input CSV file path.")
    p.add_argument("-o", "--out", type=Path, default=None, help="Optional output image path (e.g., heatmap.png).")
    p.add_argument("--delimiter", default=",", help="CSV delimiter (default: ',').")
    p.add_argument("--title", default="", help="Plot title.")
    p.add_argument("--vmin", type=float, default=None, help="Color scale minimum (default: auto).")
    p.add_argument("--vmax", type=float, default=None, help="Color scale maximum (default: auto).")
    p.add_argument("--cmap", default="viridis", help="Matplotlib colormap (default: viridis).")
    p.add_argument("--dpi", type=int, default=120, help="Figure DPI (default: 120).")
    p.add_argument("--allow-ragged", action="store_true", help="Allow ragged lines by padding shorter ones with zeros.")
    p.add_argument("--no-show", action="store_true", help="Do not open an interactive window; only save if --out is set.")
    p.add_argument("--log-transform", default=True, action="store_true",
                   help="Apply log transform (log1p) to the counters before plotting.")
    p.add_argument("--time-period", type=int, default=1000,
                   help="Time period each CSV line represents in milliseconds (default: 1000 = 1 second).")

    args = p.parse_args()

    if not args.csv.exists():
        print(f"Error: file not found: {args.csv}", file=sys.stderr)
        sys.exit(1)

    try:
        data = read_csv_as_columns(args.csv, delimiter=args.delimiter, allow_ragged=args.allow_ragged)
    except Exception as e:
        print(f"Failed to read CSV: {e}", file=sys.stderr)
        sys.exit(2)

    # Apply log transform if requested
    if args.log_transform:
        data = np.log1p(data)  # log(1 + x), safe for zeros

    plot_heatmap(
        data=data,
        title=args.title,
        out_path=args.out,
        show=(not args.no_show),
        vmin=args.vmin,
        vmax=args.vmax,
        cmap=args.cmap,
        dpi=args.dpi,
        time_period_ms=args.time_period,
    )


if __name__ == "__main__":
    main()