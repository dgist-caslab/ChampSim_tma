import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
import glob
import shutil
import time
import pandas as pd
from datetime import datetime
from common import *

# Configuration Parameters
DEFAULT_DRAM_ROWS=32768 # it makes 4GB DRAM

SIM_ROOT = "/home/phw/workspace/simulator/ChampSim_tma"
VMEM_CODE = "./inc/vmem.h"
# SPEC_TRACE_ROOT = "/home/phw/workspace/traces/Champsim/spec/"
LOG_ROOT = "/home/phw/workspace/simulator/logs/last_exp"
NUM_WARMUP = 250000000  # 250M
NUM_SIMUL = 1000000000  # 1B
EXE_CLUSTER = 24
# EXP_TIME = datetime.now().strftime("%y%m%d%H%M")

def calculate_dram_rows(target_memory_mb):
    rows_per_mb = DEFAULT_DRAM_ROWS / 4096
    rows_required = int(target_memory_mb * rows_per_mb)
    return rows_required

def run_champsim(bin, workload, trace_file):
    trace_name = os.path.basename(trace_file).replace(".champsimtrace.xz", "")
    log_file = f"{LOG_ROOT}/{trace_name}.log"
    cmd = [
        bin,
        "--warmup_instructions", str(NUM_WARMUP),
        "--simulation_instructions", str(NUM_SIMUL),
        trace_file,
        ">",
        log_file,
        "2>&1"
    ]
    try:
        subprocess.run(" ".join(cmd), shell=True, check=True)
        # print(f"run_test: cmd: {' '.join(cmd)}")
    except subprocess.CalledProcessError as e:
        print(f"Error occurred while executing the command: {e}")

# Main script to run simulations in parallel
def main():
    # ready phase
    df = pd.read_csv("./scripts/workload_memory_usage.csv")
    df = df[df['usage'] >= 100]
    df['near_dram_mb'] = df['usage'] * 0.5
    for(idx, row) in df.iterrows():
        rows = calculate_dram_rows(row['near_dram_mb'])
        # print(f"Workload: {row['workload']}, Memory: {row['near_dram_mb']}, Rows: {rows}")
        skip_make = check_bin_exists(int(row['near_dram_mb']))
        if skip_make:
            # print(f"Bin already exists : ./bin/champsim_{int(row['near_dram_mb'])}")
            df.at[idx, 'bin'] = f"./bin/champsim_{int(row['near_dram_mb'])}"
            continue
        row_configured = update_physical_fast_memory_rows("/home/phw/workspace/simulator/ChampSim_tma/tma_config.json", rows)
        code_configured = config_champsim()
        if row_configured and code_configured:
            maked = do_make()
            bin_stored = save_bin(int(row['near_dram_mb']))
            if maked and bin_stored:
                # print("Make and store bin success")
                df.at[idx, 'bin'] = f"./bin/champsim_{int(row['near_dram_mb'])}"

    # exp phase
    with ThreadPoolExecutor(EXE_CLUSTER) as executor:
        future_to_workload = {}
        for(idx, row) in df.iterrows():
            trace_file = f"/home/phw/workspace/traces/Champsim/spec/{row['workload']}.champsimtrace.xz"
            exist_bin = row['bin']
            exist_trace = os.path.exists(trace_file)
            if exist_bin and exist_trace:
                # print(f"bin: {row['bin']}, trace: {trace_file}")
                # run_chmampsim(row['bin'], row['workload'], trace_file)
                future = executor.submit(run_champsim, row['bin'], row['workload'], trace_file)
                future_to_workload[future] = row['workload']
            else:
                print(f"Bin or Trace not found for {row['workload']}")
                # abort the simulation

        # handle
        for future in future_to_workload:
            workload = future_to_workload[future]
            try:
                future.result()
            except Exception as e:
                print(f"Error occurred while executing the command: {e}")
                # abort the simulation

if __name__ == "__main__":
    main()
