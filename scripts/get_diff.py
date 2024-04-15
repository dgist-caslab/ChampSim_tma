#!/usr/bin/python3
import subprocess
import os

changed_files = subprocess.check_output(['git', 'diff', '--name-only']).decode().split()

files_with_times = []

for file in changed_files:
    stat = os.stat(file)
    files_with_times.append((file, stat.st_mtime))

files_with_times.sort(key=lambda x: x[1])

for file, _ in files_with_times:
    print(f"Changes in {file}:")
    print(subprocess.check_output(['git', 'diff', file]).decode())
    print("---------------------------------------------------")

