#!/usr/bin/python3
import os
import re
import csv

# List of directories
directories = [
    "origin_no_pref",
    "origin_pref_d2",
    "origin_pref_d4",
    "origin_pref_d8",
    "origin_pref_d16",
    "origin_pref_dyn",
]

# Regular expression pattern to extract cycles from the log file
cycle_pattern = re.compile(r'^CPU 0 cumulative IPC: .*? cycles: (\d+)', re.MULTILINE)

# Regular expression pattern to extract workload name
workload_pattern = re.compile(r'^(.*?B)_')

# List to store results
results = []

# Load sensitive workloads from file
with open('/home/phw/workspace/simulator/ChampSim_origin/scripts/sensitive_workloads', 'r') as f:
    sensitive_workloads = set(line.strip() for line in f if line.strip())

# Iterate over each directory
for directory in directories:
    # Walk through all files in the directory
    for root, _, files in os.walk(directory):
        for file in files:
            # Process only log files
            if file.endswith(".log"):
                match = workload_pattern.match(file)
                if match:
                    workload_name = match.group(1)
                    # Check if the workload is in the sensitive list
                    if workload_name in sensitive_workloads:
                        log_path = os.path.join(root, file)
                        with open(log_path, 'r') as log_file:
                            log_content = log_file.read()
                            cycle_match = cycle_pattern.search(log_content)
                            if cycle_match:
                                cycles = int(cycle_match.group(1))
                                results.append([directory, workload_name, cycles])

# Save results to a CSV file
csv_file_path = "simulation_cycles_sensitive.csv"
with open(csv_file_path, 'w', newline='') as csv_file:
    writer = csv.writer(csv_file)
    writer.writerow(["Directory", "Workload", "Cycles"])
    writer.writerows(results)

print(f"Results have been saved to {csv_file_path}")
