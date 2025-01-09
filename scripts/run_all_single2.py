import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
import glob
import shutil
import time
import re
import pandas as pd
from datetime import datetime
from common import *

# Configuration Parameters
DEFAULT_DRAM_ROWS=32768 # it makes 4GB DRAM

SIM_ROOT = "/home/phw/workspace/simulator/ChampSim_tma"
# SPEC_TRACE_ROOT = "/home/phw/workspace/traces/Champsim/spec/"
LOG_ROOT = "/home/phw/workspace/simulator/logs/last_exp"
NUM_WARMUP = 250000000  # 250M
NUM_SIMUL = 1000000000  # 1B
EXE_CLUSTER = 24
# EXP_TIME = datetime.now().strftime("%y%m%d%H%M")

def update_vmem_h_file(hit_rate, bits, file_path="/home/phw/workspace/simulator/ChampSim_tma/inc/vmem.h"):
    """
    Updates the `prefetch_hit_rate_thd` and `hit_cache_block_thd` values in the vmem.h file.

    Parameters:
        hit_rate (float): The new value for `prefetch_hit_rate_thd`.
        bits (int): The new value for `hit_cache_block_thd`.
        file_path (str): The path to the vmem.h file. Default is the standard path.
    """
    # Define patterns to search for the specific lines
    hit_rate_pattern = r"^\s*double\s+prefetch_hit_rate_thd\s*=\s*[0-9.]+;"
    bits_pattern = r"^\s*int\s+hit_cache_block_thd\s*=\s*[0-9]+;"

    # Replacement strings
    hit_rate_replacement = f"   double prefetch_hit_rate_thd = {hit_rate:.2f};"
    bits_replacement = f"   int hit_cache_block_thd = {bits};"

    try:
        # Read the file content
        with open(file_path, "r") as file:
            content = file.readlines()

        # Update the lines
        updated_content = []
        for line in content:
            if re.match(hit_rate_pattern, line):
                updated_content.append(hit_rate_replacement + "\n")
            elif re.match(bits_pattern, line):
                updated_content.append(bits_replacement + "\n")
            else:
                updated_content.append(line)

        # Write the updated content back to the file
        with open(file_path, "w") as file:
            file.writelines(updated_content)
        print(f"File updated successfully: {file_path}, {hit_rate:0.2f}, {bits}")
    except Exception as e:
        print(f"An error occurred while updating the file: {e}")


def calculate_dram_rows(target_memory_mb):
    rows_per_mb = DEFAULT_DRAM_ROWS / 4096
    rows_required = int(target_memory_mb * rows_per_mb)
    return rows_required

def run_champsim_with_feed(bin, workload, trace_file, feed_file):
    trace_name = os.path.basename(trace_file).replace(".champsimtrace.xz", "")
    log_file = f"{LOG_ROOT}/{trace_name}.log"
    cmd = [
        bin,
        "--warmup_instructions", str(NUM_WARMUP),
        "--simulation_instructions", str(NUM_SIMUL),
        trace_file,
        "--feeds", feed_file,
        ">",
        log_file,
        "2>&1"
    ]
    try:
        subprocess.run(" ".join(cmd), shell=True, check=True)
        # print(f"run_test: cmd: {' '.join(cmd)}")
    except subprocess.CalledProcessError as e:
        print(f"Error occurred while executing the command: {e}")


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
    # workloads_to_collect = [
    #          "603.bwaves_s-2931B", "619.lbm_s-2676B", "619.lbm_s-3766B",
    #          "619.lbm_s-2677B", "619.lbm_s-4268B", "603.bwaves_s-2609B",
    #          "603.bwaves_s-1740B", "602.gcc_s-2226B", "436.cactusADM-1804B",
    #          "605.mcf_s-484B", "605.mcf_s-1554B", "403.gcc-16B",
    #          "649.fotonik3d_s-7084B", "481.wrf-455B", "459.GemsFDTD-765B",
    #          "649.fotonik3d_s-1176B", "649.fotonik3d_s-8225B", "459.GemsFDTD-1418B",
    #          "459.GemsFDTD-1320B", "654.roms_s-523B", "459.GemsFDTD-1169B",
    #          "459.GemsFDTD-1211B"

    # ]
    df = pd.read_csv("./scripts/workload_memory_usage.csv")
    df_combi = pd.read_csv("/home/phw/workspace/scripts/about_trace/feed_info/found_combinations.csv")
    df = df[df['usage'] >= 50]
    # df = df[df['workload'].isin(workloads_to_collect)]
    df = df[df['workload'].isin(df_combi['workload'])]
    df['near_dram_mb'] = df['usage'] * 0.5
    print(str(len(df)))

    df = pd.merge(df, df_combi, on='workload', how='right')
    for(idx, row) in df.iterrows():
        rows = calculate_dram_rows(row['near_dram_mb'])
        # print(f"Workload: {row['workload']}, Memory: {row['near_dram_mb']}, Rows: {rows}")
        skip_make = check_bin_exists(int(row['near_dram_mb']))
        if skip_make:
            print(f"Bin already exists : ./bin/champsim_{int(row['near_dram_mb'])}")
            df.at[idx, 'bin'] = f"./bin/champsim_{row['workload']}"
            continue
        row_configured = update_physical_fast_memory_rows("/home/phw/workspace/simulator/ChampSim_tma/tma_config.json", rows)
        code_configured = config_champsim()
        if row_configured and code_configured:
            update_vmem_h_file(row['hit_rate'], row['bits'])
            maked = do_make()
            bin_stored = save_bin(row['workload'])
            if maked and bin_stored:
                # print("Make and store bin success")
                df.at[idx, 'bin'] = f"./bin/champsim_{row['workload']}"

    # exp phase
    with ThreadPoolExecutor(EXE_CLUSTER) as executor:
        future_to_workload = {}
        for(idx, row) in df.iterrows():
            trace_file = f"/home/phw/workspace/traces/Champsim/spec/{row['workload']}.champsimtrace.xz"
            feed_file = f"/home/phw/workspace/scripts/about_trace/feed_info/final_feed/{row['workload']}.csv"
            # feed_file = f"/home/phw/workspace/scripts/about_trace/parsed_bingo_output/{row['workload']}.csv"
            exist_bin = row['bin']
            exist_trace = os.path.exists(trace_file)
            if exist_bin and exist_trace:
                # print(f"bin: {row['bin']}, trace: {trace_file}")
                # run_chmampsim(row['bin'], row['workload'], trace_file)
                # future = executor.submit(run_champsim, row['bin'], row['workload'], trace_file)
                future = executor.submit(run_champsim_with_feed, row['bin'], row['workload'], trace_file, feed_file)
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
