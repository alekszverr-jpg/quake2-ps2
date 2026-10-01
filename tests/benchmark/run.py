"""Build the coordinator test with the real legacy CL_Drop implementation."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / "src/client/cl_main.c").read_text()
match = re.search(r"^void CL_Drop\(void\)\n\{.*?^\}", source, re.M | re.S)
if not match:
    raise RuntimeError("Cannot locate production CL_Drop for regression test")
output = root / "build/benchmark-host"
output.mkdir(parents=True, exist_ok=True)
(output / "benchmark_drop.inc").write_text(match.group(0))
binary = output / ("test.exe" if os.name == "nt" else "test")
flags = [] if os.name == "nt" else ["-fsanitize=address,undefined"]
subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu89", "-Wall", "-Wextra",
                "-Werror", '-DPS2_BUILD_VERSION="' + (root/'VERSION').read_text().strip() + '"',
                *flags, "-I" + str(root / "src"), "-I" + str(output),
                str(root / "tests/benchmark/test.c"), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
