"""Exercise production allocation retry and lighting-cache invalidation."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / "build/memory-reclaim-host"
out.mkdir(parents=True, exist_ok=True)
functions = []
for path, signatures in [
    ("heap.cpp", ["static bool ReclaimOptionalMemory", "void PS2_SetMemoryReclaimer", "void * PS2_MemAlloc", "void * PS2_MemAllocAligned", "void * PS2_MemTryAllocAligned"]),
    ("../renderer/render_view.cpp", ["void ResetLitTriangle", "void ClearLitTriangleCaches", "int ReclaimLightingCache"]),
]:
    source = (root / "src/ps2/system" / path).read_text()
    for signature in signatures:
        match = re.search(r"^" + re.escape(signature) + r"\([^\n]*\)\n\{.*?^\}", source, re.M | re.S)
        if not match: raise RuntimeError("Cannot extract " + signature)
        functions.append(match.group(0))
(out / "reclaim.inc").write_text("\n".join(functions))
binary = out / ("test.exe" if os.name == "nt" else "test")
flags = [] if os.name == "nt" else ["-fsanitize=address,undefined"]
subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                *flags, "-I" + str(root / "src"), "-I" + str(out),
                str(root / "tests/memory_reclaim/test.cpp"), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
