"""Run host regression tests against the real drive I/O module and MQTT formatter."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--cxx", default="g++", help="Host C++ compiler executable")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
compiler = Path(args.cxx)
if compiler.is_file():
    os.environ["PATH"] = str(compiler.resolve().parent) + os.pathsep + os.environ.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="gdc-tests-") as build:
    executable = Path(build) / "command_safety.exe"
    subprocess.run([
        args.cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "tests/host/stubs"), "-I" + str(root / "include"),
        str(root / "tests/host/command_safety.cpp"), str(root / "src/driveio.cpp"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
