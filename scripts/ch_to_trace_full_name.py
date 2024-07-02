#!/usr/bin/python3
import re

# Define the trace name to full name mapping
trace_name_to_full_name = {
    "458.sjeng": "458.sjeng-1088B.champsimtrace.xz",
    "649.fotonik3d_s": "649.fotonik3d_s-10881B.champsimtrace.xz",
    "620.omnetpp_s": "620.omnetpp_s-874B.champsimtrace.xz",
    "625.x264_s": "625.x264_s-39B.champsimtrace.xz",
    "400.perlbench": "400.perlbench-50B.champsimtrace.xz",
    "436.cactusADM": "436.cactusADM-1804B.champsimtrace.xz",
    "410.bwaves": "410.bwaves-2097B.champsimtrace.xz",
    "435.gromacs": "435.gromacs-228B.champsimtrace.xz",
    "464.h264ref": "464.h264ref-97B.champsimtrace.xz",
    "483.xalancbmk": "483.xalancbmk-736B.champsimtrace.xz",
    "648.exchange2_s": "648.exchange2_s-1712B.champsimtrace.xz",
    "433.milc": "433.milc-337B.champsimtrace.xz",
    "437.leslie3d": "437.leslie3d-273B.champsimtrace.xz",
    "641.leela_s": "641.leela_s-1083B.champsimtrace.xz",
    "627.cam4_s": "627.cam4_s-573B.champsimtrace.xz",
    "482.sphinx3": "482.sphinx3-1522B.champsimtrace.xz",
    "453.povray": "453.povray-887B.champsimtrace.xz",
    "600.perlbench_s": "600.perlbench_s-1273B.champsimtrace.xz",
    "602.gcc_s": "602.gcc_s-2375B.champsimtrace.xz",
    "444.namd": "444.namd-426B.champsimtrace.xz",
    "450.soplex": "450.soplex-247B.champsimtrace.xz",
    "657.xz_s": "657.xz_s-4994B.champsimtrace.xz",
    "621.wrf_s": "621.wrf_s-8100B.champsimtrace.xz",
    "654.roms_s": "654.roms_s-1613B.champsimtrace.xz",
    "429.mcf": "429.mcf-217B.champsimtrace.xz",
    "456.hmmer": "456.hmmer-327B.champsimtrace.xz",
    "403.gcc": "403.gcc-48B.champsimtrace.xz",
    "481.wrf": "481.wrf-1281B.champsimtrace.xz",
    "445.gobmk": "445.gobmk-36B.champsimtrace.xz",
    "605.mcf_s": "605.mcf_s-1644B.champsimtrace.xz",
    "603.bwaves_s": "603.bwaves_s-5359B.champsimtrace.xz",
    "454.calculix": "454.calculix-460B.champsimtrace.xz",
    "628.pop2_s": "628.pop2_s-17B.champsimtrace.xz",
    "459.GemsFDTD": "459.GemsFDTD-1491B.champsimtrace.xz",
    "623.xalancbmk_s": "623.xalancbmk_s-700B.champsimtrace.xz",
    "607.cactuBSSN_s": "607.cactuBSSN_s-4248B.champsimtrace.xz",
    "638.imagick_s": "638.imagick_s-10316B.champsimtrace.xz",
    "416.gamess": "416.gamess-875B.champsimtrace.xz",
    "644.nab_s": "644.nab_s-12521B.champsimtrace.xz",
    "447.dealII": "447.dealII-3B.champsimtrace.xz",
    "471.omnetpp": "471.omnetpp-188B.champsimtrace.xz",
    "401.bzip2": "401.bzip2-277B.champsimtrace.xz",
    "434.zeusmp": "434.zeusmp-10B.champsimtrace.xz",
    "462.libquantum": "462.libquantum-1343B.champsimtrace.xz",
    "473.astar": "473.astar-359B.champsimtrace.xz",
    "619.lbm_s": "619.lbm_s-4268B.champsimtrace.xz",
    "470.lbm": "470.lbm-1274B.champsimtrace.xz",
    "465.tonto": "465.tonto-1914B.champsimtrace.xz",
    "631.deepsjeng_s": "631.deepsjeng_s-928B.champsimtrace.xz",
}

# Function to update the trace names in a line
def update_trace_names_in_line(line, trace_mapping):
    pattern = re.compile(r"\('(.+?)'\)")
    match = pattern.search(line)
    if match:
        original_combination = match.group(1)
        traces = original_combination.split("', '")
        updated_traces = [trace_mapping.get(trace, trace) for trace in traces]
        updated_combination = f"('{', '.join(updated_traces)}')"
        return line.replace(match.group(0), updated_combination)
    return line

# Input and output file paths
input_file_path = '/home/phw/workspace/simulator/scripts/valid_combi.dat'
output_file_path = '/home/phw/workspace/simulator/scripts/updated_combi.dat'

# Process the input file and write to the output file
with open(input_file_path, 'r') as infile, open(output_file_path, 'w') as outfile:
    for line in infile:
        updated_line = update_trace_names_in_line(line, trace_name_to_full_name)
        outfile.write(updated_line)

print(f"Updated file saved to {output_file_path}")
