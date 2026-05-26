#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import argparse

def parse_args():
    parser = argparse.ArgumentParser(description='Process migration statistics CSV file.')
    parser.add_argument('csv_file', type=str, help='Path to the CSV file to process')
    parser.add_argument('--output', type=str, help='Output file for the plots (if not provided, plots are shown on screen)')
    return parser.parse_args()

def main():
    args = parse_args()
    
    # Read the headerless CSV file and assign column names
    column_names = ["migrations_up", "migrations_down", "migration_waits", "migration_wait_us"]
    df = pd.read_csv(args.csv_file, header=None, names=column_names)

    df['migration_wait_ms'] = df['migration_wait_us'] / 1000 
    
    # Calculate deltas between consecutive rows
    delta_df = df.diff()
    
    # Drop the first row which contains NaN values after diff operation
    delta_df = delta_df.dropna()

    delta_df["migrations_up"].to_csv("migrations_up.txt", header=False, index=False)
    
    # Create a figure with two subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
    
    # Plot migrations_up and migrations_down on the first subplot
    ax1.plot(delta_df.index, delta_df['migrations_up'], 'b-', label='Migrations Up')
    ax1.plot(delta_df.index, delta_df['migrations_down'], 'r-', label='Migrations Down')
    ax1.set_xlabel('Seconds')
    ax1.set_ylabel('Pages migrated')
    ax1.set_title('Numbers of Migrations Up and Down')
    ax1.legend()
    ax1.grid(True)
    
    # Plot migration_waits and migration_wait_us on the second subplot with two y-axes
    ax2_wait = ax2
    ax2_wait.plot(delta_df.index, delta_df['migration_waits'], 'g-', label='Migration Waits')
    ax2_wait.set_xlabel('Seconds')
    ax2_wait.set_ylabel('Delta Wait Count', color='g')
    ax2_wait.tick_params(axis='y', labelcolor='g')
    
    # Create a second y-axis for migration_wait_us
    ax2_wait_us = ax2.twinx()
    ax2_wait_us.plot(delta_df.index, delta_df['migration_wait_ms'], 'm-', label='Migration Wait ms')
    ax2_wait_us.set_ylabel('Delta Wait Time (ms)', color='m')
    ax2_wait_us.tick_params(axis='y', labelcolor='m')
    
    # Add a combined legend for both lines in the second subplot
    lines1, labels1 = ax2_wait.get_legend_handles_labels()
    lines2, labels2 = ax2_wait_us.get_legend_handles_labels()
    ax2.legend(lines1 + lines2, labels1 + labels2, loc='upper right')
    
    ax2.grid(True)
    ax2.set_title('Migration Waits and Wait Time (ms)')
    
    plt.tight_layout()
   
    plt.savefig("migration_stats.png") 
    # Either save the plot to a file or display it
    # if args.output:
    #     plt.savefig(args.output)
    #     print(f"Plot saved to {args.output}")
    # else:
    #     plt.show()

if __name__ == "__main__":
    main()