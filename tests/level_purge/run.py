"""Check the production pre-BSP purge and model registration lifecycle."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
output = root / "build/level-purge-host"
output.mkdir(parents=True, exist_ok=True)
functions = []
for path, signature in [
    ("ref.cpp", 'extern "C" void PS2_PurgeLevelRendererMemory()'),
    ("model.cpp", "void ModelCache::PurgeLevelModels()"),
]:
    source = (root / "src/ps2/renderer" / path).read_text()
    match = re.search(r"^" + re.escape(signature) + r"\n\{.*?^\}",
                      source, re.M | re.S)
    if not match:
        raise RuntimeError("Cannot extract " + signature)
    functions.append(match.group(0))
(output / "purge.inc").write_text("\n".join(functions))
binary = output / ("test.exe" if os.name == "nt" else "test")
flags = [] if os.name == "nt" else ["-fsanitize=address,undefined"]
subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra",
                "-Werror", *flags, "-I" + str(output),
                str(root / "tests/level_purge/test.cpp"), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
