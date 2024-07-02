#!/usr/bin/python3
import os
import re
import sys
from datetime import datetime

def get_build_id(build_config_path):
    with open(build_config_path, 'r') as file:
        build_config_content = file.read()
    return re.search(r'(?<=# Build ID: ).*', build_config_content).group(0)

def get_sim_num_cpu(constants_file_path):
    with open(constants_file_path, 'r') as file:
        constants_content = file.read()
    return int(re.search(r'(?<=constexpr std::size_t NUM_CPUS = )\d+', constants_content).group(0))

def construct_command(sim_root, num_warmup, num_simul, concated_trace, log_file_path):
    return f"{sim_root}/bin/champsim --warmup-instructions {num_warmup} --simulation-instructions {num_simul} {concated_trace} > {log_file_path} 2>&1"

def main():
    SIM_ROOT = "/home/phw/workspace/simulator/ChampSim_tma"
    SPEC_TRACE_ROOT = "/home/phw/sim/traces/downloads"
    LOG_ROOT = "/home/phw/workspace/simulator/logs"
    BUILD_CONFIG = os.path.join(SIM_ROOT, "_configuration.mk")

    BUILD_ID = get_build_id(BUILD_CONFIG)
    
    CONSTANTS_FILE = os.path.join(SIM_ROOT, f".csconfig/{BUILD_ID}/inc/champsim_constants.h")
    SIM_NUM_CPU = get_sim_num_cpu(CONSTANTS_FILE)

    NUM_WARMUP = 100000000    # 1B
    NUM_SIMUL = 200000000     # 2B
    TRACE = ""
    CONCATED_TRACE = ""
    EXP_TIME = datetime.now().strftime("%y%m%d%H%M")

    if len(sys.argv) < 3:
        print("enter target trace")
        print("usage: run_sim.py <trace_name> <num_cpu> [NUM_WARMUP;1B] [NUM_SIMUL;5B]")
        sys.exit()

    TRACE = sys.argv[1]
    NUM_CPU = int(sys.argv[2])
    for i in range(NUM_CPU):
        CONCATED_TRACE += f"{SPEC_TRACE_ROOT}/{TRACE} "

    if len(sys.argv) > 3:
        NUM_WARMUP = int(sys.argv[3])
    if len(sys.argv) > 4:
        NUM_SIMUL = int(sys.argv[4])
    
    if NUM_CPU != SIM_NUM_CPU:
        print(f"target number of trace({NUM_CPU}) is not equal to the number of CPUs in the simulator({SIM_NUM_CPU})")
        print("need re-compile simulator")
        sys.exit()

    # execution command
    log_file_path = os.path.join(LOG_ROOT, f"{TRACE}_{EXP_TIME}.log")
    with open(log_file_path, 'w') as log_file:
        log_file.write(f"BUILD_ID: {BUILD_ID}\n")

    command = construct_command(SIM_ROOT, NUM_WARMUP, NUM_SIMUL, CONCATED_TRACE, log_file_path)
    os.system(command)

if __name__ == "__main__":
    main()