#!/bin/bash

# Define the root paths
SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
# TRACE_FILE=/home/phw/sim/traces/downloads/649.fotonik3d_s-10881B.champsimtrace.xz  # Sample trace file path
TRACE_FILE=/home/phw/sim/traces/downloads/602.gcc_s-2375B.champsimtrace.xz  # Sample trace file path
# LOG_FILE=/home/phw/workspace/simulator/logs/last_exp/sim_test.log

# Define the warmup and simulation instructions
NUM_WARMUP=10000000    # 10M instructions for warmup
NUM_SIMUL=25000000     # 25M instructions for simulation

# Run the simulator with a sample trace
run_simulator() {
    local trace_file=$1
    echo "Running simulator with trace: $trace_file"
    echo "$SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file"

    # Execute the simulator and log the output
    $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file

    if [ $? -eq 0 ]; then
        echo "Simulator executed successfully."
    else
        echo "Simulator execution failed."
    fi
}

# Run the simulator with the provided trace and setup
run_simulator $TRACE_FILE
