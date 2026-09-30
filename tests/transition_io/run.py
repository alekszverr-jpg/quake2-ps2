"""Exercise production filesystem functions and pre-map PCM purge on host."""
import os
from pathlib import Path
import re
import subprocess
import uuid

root = Path(__file__).resolve().parents[2]
out = root / "build/transition-io-host"
out.mkdir(parents=True, exist_ok=True)
functions = []
for path, signatures in [
    ("src/ps2/system/sys.cpp", ["void Sys_Mkdir", "int Sys_PrepareSaveFile", "char * Sys_FindFirst", "char * Sys_FindNext", "void Sys_FindClose"]),
    ("src/common/filesys.c", ["void FS_CreatePath"]),
    ("src/server/sv_ccmds.c", ["void SV_WipeSavegame"]),
    ("src/client/snd_dma.c", ["void S_PurgeLevelSounds"]),
]:
    source = (root / path).read_text()
    for signature in signatures:
        match = re.search(r"^" + re.escape(signature) + r"\([^\n]*\)(?:\n\{.*?^\}| \{[^\n]*\})", source, re.M | re.S)
        if not match:
            raise RuntimeError("Cannot extract " + signature)
        functions.append(match.group(0))
(out / "transition.inc").write_text("\n".join(functions))
binary = out / ("test.exe" if os.name == "nt" else "test")
flags = [] if os.name == "nt" else ["-fsanitize=address,undefined"]
subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                *flags, "-I" + str(root / "src"), "-I" + str(out),
                str(root / "tests/transition_io/test.cpp"), "-o", str(binary)], check=True)
subprocess.run([str(binary), str(out / ("fixture-" + uuid.uuid4().hex[:8]))], check=True)
