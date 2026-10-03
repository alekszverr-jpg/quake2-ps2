"""Exercise real model profiling scopes and the production submission boundary."""
import os
from pathlib import Path
import re
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / "build/model-profile-host"
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/ps2/renderer/render_view.cpp").read_text()
match = re.search(r"^inline void FlushScratch\(.*?^\}", source, re.M | re.S)
if not match: raise RuntimeError("FlushScratch")
(out / "flush.inc").write_text(match.group(0))
binary = out / ("test.exe" if os.name == "nt" else "test")
flags = [] if os.name == "nt" else ["-fsanitize=address,undefined"]
subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags,
    "-I" + str(root / "tests/model_profile/stubs"), "-I" + str(root / "src"),
    "-I" + str(out), str(root / "tests/model_profile/test.cpp"), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
