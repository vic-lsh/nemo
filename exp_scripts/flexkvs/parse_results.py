#!/usr/bin/env python3
"""
Parse a workload log and produce:
  - throughput_<name>.txt : one throughput (mops) per line
  - latency_<name>.txt    : one "<percentile>,<latency>" per line (from the last TP line)

Usage:
  python parse_results.py /path/to/log.txt  [--name RUNNAME]

If --name is omitted, you'll be prompted for it. Empty input errors out.
"""

import argparse
import re
import sys
from pathlib import Path

TP_THROUGHPUT_RE = re.compile(r'^TP:\s+total=([0-9.]+)\s+mops\b')
TP_PCTS_RE = re.compile(
    r'50p=(-?\d+)\s+us.*?90p=(-?\d+)\s+us.*?95p=(-?\d+)\s+us.*?'
    r'99p=(-?\d+)\s+us.*?99\.9p=(-?\d+)\s+us.*?99\.99p=(-?\d+)\s+us'
)
FINAL_TPUT_RE = re.compile(r'^Final throughput\s*=\s*([0-9.]+)\s+mops\b')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logfile", help="Path to the log file to parse")
    ap.add_argument("--name", help="Short name used in output filenames")
    args = ap.parse_args()

    name = args.name
    if not name:
        try:
            name = input("Enter <name> for output files (required): ").strip()
        except EOFError:
            name = ""
    if not name:
        print("Error: <name> is required (e.g., --name run1).", file=sys.stderr)
        sys.exit(2)

    log_path = Path(args.logfile)
    if not log_path.is_file():
        print(f"Error: file not found: {log_path}", file=sys.stderr)
        sys.exit(1)

    throughputs = []          # list of strings as they appear, e.g., "4.1303"
    last_pcts = None          # tuple of strings (p50,p90,p95,p99,p999,p9999)

    with log_path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")

            # Collect per-interval throughput
            m = TP_THROUGHPUT_RE.search(line)
            if m:
                throughputs.append(m.group(1))

                # Keep updating percentiles; we'll output the last seen set
                mp = TP_PCTS_RE.search(line)
                if mp:
                    last_pcts = mp.groups()
                continue

            # Optionally include the final throughput as another line
            m2 = FINAL_TPUT_RE.search(line)
            if m2:
                throughputs.append(m2.group(1))
                continue

    # Write throughput_<name>.txt
    tput_out = Path(f"throughput_{name}.txt")
    with tput_out.open("w", encoding="utf-8") as fo:
        for v in throughputs:
            fo.write(f"{v}\n")

    # Write latency_<name>.txt from the last seen TP line’s percentiles (if any)
    # Format: "<percentile>,<latency>"
    lat_out = Path(f"latency_{name}.txt")
    with lat_out.open("w", encoding="utf-8") as fo:
        if last_pcts is None:
            # Nothing parsed — create an empty file but make it obvious on stderr.
            print("Warning: no TP percentile lines found; latency file will be empty.", file=sys.stderr)
        else:
            labels = ["50", "90", "95", "99", "99.9", "99.99"]
            for label, val in zip(labels, last_pcts):
                fo.write(f"{label},{val}\n")

    print(f"Wrote: {tput_out} and {lat_out}")

if __name__ == "__main__":
    main()
